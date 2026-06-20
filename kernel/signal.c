#include <asm-i386/ptrace.h>
#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/slab.h>
#include <kfs/stddef.h>

/** entry.S が sys_sigreturn のために保存する ring-0 の pt_regs ポインタ
 * @note シングル CPU なのでグローバルで安全
 */
struct pt_regs *g_current_regs;

/** 現在実行中プロセス（kernel/sched/core.cで定義） */
extern struct task_struct *current;

/* 保留シグナルエントリ */
struct pending_signal_entry
{
	struct list_head list; /* リストノード */
	unsigned int magic;	   /* マジックナンバー */
	int sig;			   /* シグナル番号 */
};

#define PENDING_SIGNAL_MAGIC 0x53494751U /* シグナルキューのマジックナンバー */

/* シグナルキューを全解放し、保留シグナル状態を初期化する */
void signal_flush_pending(struct task_struct *task)
{
	/* タスクが存在しない場合は何もしない */
	if (!task)
	{
		return;
	}

	int guard = 0; /* 不正なリスト構造による無限ループを防ぐためのガード */
	while (!list_empty(&task->pending.list) && guard++ < (_NSIG + 4))
	{
		struct pending_signal_entry *entry =
			list_entry(task->pending.list.next, struct pending_signal_entry, list); /* リストからエントリを取得 */
		if (entry->magic != PENDING_SIGNAL_MAGIC)
		{
			/* マジックナンバーが一致しない場合は
			 * リストが壊れているとみなして終了 */
			break;
		}

		/* エントリをリストから削除して解放 */
		list_del(&entry->list);
		entry->magic = 0;
		kfree(entry);
	}

	/* シグナルキューを初期化 */
	INIT_LIST_HEAD(&task->pending.list);
	task->pending.signal = 0;
}

/** 保留シグナルキューにエントリを追加する
 * @param task 保留シグナルを持つタスク
 * @param sig  追加するシグナル番号
 * @return 成功時は0、エラー時は-1
 * @note 同一番号の多重追加はしない前提である
 */
static int signal_enqueue_pending(struct task_struct *task, int sig)
{
	struct pending_signal_entry *entry = kmalloc(sizeof(*entry));
	if (!entry)
	{
		return -1;
	}

	entry->sig = sig;
	entry->magic = PENDING_SIGNAL_MAGIC; /* マジックナンバーを設定 */
	INIT_LIST_HEAD(&entry->list);
	list_add_tail(&entry->list, &task->pending.list);
	return 0;
}

/** 保留シグナルキューから次のシグナルを1つ取り出す
 * @param task 保留シグナルを持つタスク
 * @param sig  取り出したシグナル番号を格納するポインタ
 * @return 取り出せた場合は1、取り出せなかった場合は0
 */
static int signal_dequeue_pending(struct task_struct *task, int *sig)
{
	/* タスクが空またはシグナルポインタが無効な場合は何もしない */
	if (!task || !sig)
	{
		return 0;
	}

	/* 保留シグナルキューにエントリが存在する場合は先頭から取り出す */
	if (!list_empty(&task->pending.list))
	{
		struct pending_signal_entry *entry = list_entry(task->pending.list.next, struct pending_signal_entry, list);
		/* マジックナンバーが一致しない場合はリストが壊れているとみなして初期化 */
		if (entry->magic != PENDING_SIGNAL_MAGIC)
		{
			INIT_LIST_HEAD(&task->pending.list);
			return 0;
		}

		/* エントリからシグナル番号を取得 */
		*sig = entry->sig;

		/* エントリをリストから削除して解放 */
		list_del(&entry->list);
		entry->magic = 0;
		kfree(entry);
		task->pending.signal &= ~(1UL << *sig);
		return 1;
	}

	/* 旧経路互換: bitmask のみ立っている場合は従来通り番号順で処理 */
	for (int i = 1; i < _NSIG; i++)
	{
		if (task->pending.signal & (1UL << i))
		{
			*sig = i;
			task->pending.signal &= ~(1UL << i);
			return 1;
		}
	}

	return 0;
}

/** シグナル番号が有効範囲内かを検証する
 * @param sig 検証するシグナル番号
 * @return 有効なら1，無効なら0
 */
static int valid_signal(int sig)
{
	/* シグナル番号は1からNSIG-1まで有効 */
	return sig > 0 && sig < _NSIG;
}

/** 現在のプロセスにシグナルを発生させる
 * @brief send_signal(sig, current) のラッパー（POSIX互換名）
 * @param sig 発生させるシグナル番号
 * @return 成功時は0、エラー時は-1
 */
int raise(int sig)
{
	return send_signal(sig, current);
}

/** ring-3 ハンドラが return した後に実行されるトランポリン
 * @brief arch/i386/kernel/sigreturn.S の同名スタブ。int $0x80 で sys_sigreturn を呼び元のコンテキストへ復帰する
 * @note  カーネルコードは PAGE_USER でマップされているため ring-3 から直接呼び出し可能
 */
extern void sigreturn(void);

/** ユーザスタックにシグナルフレームを構築する
 * @param regs    例外ハンドラから渡された pt_regs（iret でユーザ空間へ復帰する）
 * @param sig     配信するシグナル番号
 * @param handler ユーザ登録ハンドラ
 */
static void setup_sigframe(struct pt_regs *regs, int sig, sighandler_t handler)
{
	struct sigframe *frame;
	unsigned long user_esp;

	user_esp = regs->esp;
	user_esp -= sizeof(struct sigframe);
	user_esp &= ~3UL; /* 4バイトアライン */
	frame = (struct sigframe *)user_esp;

	frame->pretcode = (unsigned long)sigreturn; /* return 先 */
	frame->sig = sig;							/* handler の引数 */
	frame->saved_regs = *regs;					/* 復帰用コンテキスト */

	/* iret でハンドラへジャンプするよう pt_regs を書き換える */
	regs->esp = user_esp;
	regs->eip = (unsigned long)handler;
}

/** 子taskの状態変化を親へ通知する
 * @param task 状態が変化した子task
 */
static void notify_parent_of_child_state(struct task_struct *task)
{
	/* 親が存在しない、親が自分自身、親が終了している場合は通知不要 */
	if (!task->parent || task->parent == task || task->parent->exit_state == EXIT_DEAD)
	{
		return;
	}

	(void)send_signal(SIGCHLD, task->parent);
}

/** 保留中シグナルを処理する（pt_regs あり版）
 * @param regs  例外/syscall ハンドラの pt_regs。NULL の場合は ring-0 から直接呼び出す（テスト用）
 */
void do_signal_with_regs(struct pt_regs *regs)
{
	int sig;
	sighandler_t handler;

	/* 保留シグナルがなければ何もしない */
	if (current->pending.signal == 0)
	{
		return;
	}

	/* 保留キューから順に取り出して処理 */
	while (signal_dequeue_pending(current, &sig))
	{
		handler = current->sig_actions[sig].sa_handler;

		/* SIG_IGN なら無視 */
		if (handler == SIG_IGN)
		{
			continue;
		}

		/* SIG_DFL ならデフォルト動作 */
		if (handler == SIG_DFL)
		{
			/* SIGINT を含む終了系シグナルはデフォルトでプロセスを終了 */
			if (sig == SIGINT || sig == SIGKILL || sig == SIGSEGV || sig == SIGILL || sig == SIGTERM || sig == SIGFPE ||
				sig == SIGBUS)
			{
				extern __attribute__((noreturn)) void do_exit(int code);
				/* 終了コードにシグナル番号を使う（POSIX 慣習） */
				do_exit(sig);
			}

			/* 停止系シグナルのデフォルト動作: TASK_STOPPED へ遷移 */
			if (sig == SIGTSTP || sig == SIGSTOP || sig == SIGTTIN || sig == SIGTTOU)
			{
				current->exit_signal = sig;				 /* 停止シグナルを設定 */
				current->flags |= PF_WAIT_STOP_PENDING;	 /* 停止待ちフラグをセット */
				current->flags &= ~PF_WAIT_CONT_PENDING; /* 再開待ちフラグをクリア */
				current->__state = __TASK_STOPPED;		 /* プロセス状態を__TASK_STOPPEDにセット */

				/* waitpid(WUNTRACED)中の親へ停止を通知して起床させる */
				notify_parent_of_child_state(current);

				/* スケジューラを呼び出して他のプロセスに CPU を譲る */
				schedule();

				/* 復帰後はキューの先頭から続行する */
				continue;
			}

			/* SIGCONT のデフォルト動作は再開（send_signal 側で状態遷移済み） */
			if (sig == SIGCONT)
			{
				continue;
			}
			continue;
		}

		/* ユーザ定義ハンドラ */
		if (regs != (struct pt_regs *)0)
		{
			/* ring-3 実行: ユーザスタックに sigframe を構築して iret でハンドラへ */
			setup_sigframe(regs, sig, handler);
			return; /* 1回の例外で 1 シグナルのみ処理 */
		}
		else
		{
			/* フォールバック: ring-0 から直接呼び出し（テスト用 / do_signal() 互換） */
			handler(sig);
		}
	}
}

/** 保留中シグナルを処理する（カーネル内部 / テスト用）
 * @brief do_signal_with_regs(NULL) のラッパ — ring-0 から直接ハンドラを呼ぶ
 */
void do_signal(void)
{
	do_signal_with_regs((struct pt_regs *)0);
}

/** sys_sigreturn: シグナルハンドラ実行後に元のコンテキストへ復帰する
 * @brief sigreturn() から int $0x80 で呼ばれる（arch/i386/kernel/sigreturn.S）
 * @note  g_current_regs は entry.S が syscall 入り口で保存した ring-0 の pt_regs ポインタ
 *
 * 呼び出し時のスタックレイアウト（ring-3 esp 時）:
 *   esp+0 : frame->sig  (ハンドラ return 後に ret がポップした後）
 *   esp+4 : frame->saved_regs
 */
int sys_sigreturn(void)
{
	struct pt_regs *kstack = g_current_regs;
	/* ハンドラ return 後の ring-3 esp は frameの pretcode を ret でポップした後なので
	 * esp → [sig, saved_regs, ...]。saved_regs は esp + sizeof(int) にある。 */
	struct pt_regs *saved = (struct pt_regs *)((unsigned long)kstack->esp + sizeof(int));

	*kstack = *saved;
	/* entry.S が戻り値を pt_regs->eax に書こうとするので saved->eax を返す */
	return (int)saved->eax;
}

/** シグナルが保留中かどうかを確認する
 * @return 保留中のシグナルがあれば1、なければ0
 */
int signal_pending(void)
{
	return current->pending.signal != 0;
}

/** 特定プロセスへシグナルを送信する
 * @brief 対象プロセスの保留シグナルビットマスクにシグナルをセットする
 * @param sig 送信するシグナル番号
 * @param p   送信先プロセス
 * @return 成功時は0、エラー時は-1
 */
int send_signal(int sig, struct task_struct *p)
{
	/* シグナル番号の有効性を検証 */
	if (!valid_signal(sig))
	{
		return -1;
	}

	if (p == (struct task_struct *)0)
	{
		return -1;
	}

	unsigned long mask = (1UL << sig);

	/* 非リアルタイムシグナルの場合，
	 * 同じシグナルが既に保留中であれば新たに追加しない */
	if (!(p->pending.signal & mask))
	{
		if (signal_enqueue_pending(p, sig) != 0)
		{
			return -1;
		}
		p->pending.signal |= mask;
	}

	/* SIGCONT の場合は停止中フラグをクリアして再開フラグをセット */
	if (sig == SIGCONT)
	{
		p->flags |= PF_WAIT_CONT_PENDING;  /* 再開フラグをセット */
		p->flags &= ~PF_WAIT_STOP_PENDING; /* 停止待ちフラグをクリア */
	}

	/* シグナル到来時は割り込み可能スリープ中のプロセスを起床させる */
	wake_up_process(p);

	/* waitpid(WCONTINUED)中の親へ再開を通知する */
	if (sig == SIGCONT)
	{
		notify_parent_of_child_state(p);
	}

	return 0;
}

struct pgrp_signal_ctx
{
	pid_t pgrp;	   /* 対象プロセスグループID */
	int sig;	   /* 送信するシグナル番号 */
	int matched;   /* 対象グループに属するプロセス数 */
	int delivered; /* 送信したシグナルの数 */
	int denied;	   /* 権限不足で拒否された送信数 */
	int error;	   /* 送信中にエラーが発生した場合は負のエラーコードをセット */
};

/* kill送信権限: CAP_KILL を持つか、送信元 UID/EUID が対象 UID/EUID のいずれかと一致 */
static int can_send_kill_signal(const struct task_struct *sender, const struct task_struct *target)
{
	/* 送信元または送信先が無効な場合は送信不可 */
	if (!sender || !target)
	{
		return 0;
	}

	/* 送信元と送信先が同じ場合は送信可能 */
	if (sender == target)
	{
		return 1;
	}

	/* CAP_KILL 権限を持つ場合は送信可能 */
	if (cap_raised(sender->cap_effective, CAP_KILL))
	{
		return 1;
	}

	/* UID または EUID が一致する場合は送信可能 */
	if (sender->uid.val == target->uid.val || sender->uid.val == target->euid.val ||
		sender->euid.val == target->uid.val || sender->euid.val == target->euid.val)
	{
		return 1;
	}

	return 0;
}

static int kill_pg_cb(struct task_struct *task, void *ctx)
{
	struct pgrp_signal_ctx *signal_ctx = (struct pgrp_signal_ctx *)ctx;

	if (task->exit_state == EXIT_DEAD)
	{
		return 0;
	}
	if (task->pgrp != signal_ctx->pgrp)
	{
		return 0;
	}

	/* 対象グループに属するプロセス数をカウント */
	signal_ctx->matched++;

	/* 送信権限がない場合は拒否 */
	if (!can_send_kill_signal(current, task))
	{
		signal_ctx->denied++;
		return 0;
	}

	/* シグナルを送信 */
	if (send_signal(signal_ctx->sig, task) == 0)
	{
		signal_ctx->delivered++;
	}
	else
	{
		signal_ctx->error = -EINVAL;
	}
	return 0;
}

/** 同一プロセスグループの全メンバにシグナルを送信する
 * @param pgrp 対象プロセスグループID
 * @param sig  送信するシグナル番号
 * @return 0: 成功，-ESRCH: 対象グループなし，-EINVAL: 引数不正
 */
int kill_pg(pid_t pgrp, int sig)
{
	struct pgrp_signal_ctx ctx;

	if (pgrp <= 0)
	{
		return -EINVAL;
	}
	if (!valid_signal(sig))
	{
		return -EINVAL;
	}

	ctx.pgrp = pgrp;
	ctx.sig = sig;
	ctx.matched = 0;
	ctx.delivered = 0;
	ctx.denied = 0;
	ctx.error = 0;

	/* 対象プロセスグループの全メンバにシグナルを送信 */
	if (task_for_each(kill_pg_cb, &ctx) < 0)
	{
		return -EINVAL;
	}

	/* シグナル送信中にエラーが発生した場合はエラーコードを返す */
	if (ctx.error)
	{
		return ctx.error;
	}
	/* 対象プロセスグループに属するプロセスが存在しない場合はエラーを返す */
	if (ctx.matched == 0)
	{
		return -ESRCH;
	}
	/* 対象プロセスグループに属するプロセスが存在するが、送信権限がない場合はエラーを返す */
	if (ctx.delivered == 0 && ctx.denied > 0)
	{
		return -EPERM;
	}
	return 0;
}

/** killシステムコール用ヘルパー
 * @brief 指定PIDのプロセスにシグナルを送信する
 * @param pid  送信先プロセスID
 * @param sig  送信するシグナル番号
 * @return 成功時は0、エラー時は負のエラーコード
 */
int sys_kill(pid_t pid, int sig)
{
	struct task_struct *p;
	pid_t pgrp;

	if (!valid_signal(sig))
	{
		return -EINVAL;
	}

	/* pid < 0 はプロセスグループ -pid にシグナルを送る */
	if (pid < 0)
	{
		pgrp = -pid;
		if (pgrp <= 0)
		{
			return -EINVAL;
		}
		return kill_pg(pgrp, sig);
	}

	/* pid == 0 は呼び出し元のプロセスグループを意味する */
	if (pid == 0)
	{
		if (current->pgrp <= 0)
		{
			return -ESRCH;
		}
		return kill_pg(current->pgrp, sig);
	}

	/* 送信先プロセスをPIDで検索 */
	p = find_task_by_pid(pid);
	if (p == (struct task_struct *)0)
	{
		return -ESRCH; /* プロセスが存在しない */
	}

	/* 送信権限がない場合は拒否 */
	if (!can_send_kill_signal(current, p))
	{
		return -EPERM;
	}

	/* シグナルを送信 */
	if (send_signal(sig, p) != 0)
	{
		return -EINVAL; /* 無効なシグナル番号 */
	}

	return 0;
}

/** signalシステムコール用ヘルパー
 * @brief 現在のプロセスのシグナルハンドラを設定する
 * @param sig     シグナル番号
 * @param handler 設定するハンドラ関数
 * @return 以前のハンドラ、エラー時はSIG_ERR
 */
sighandler_t sys_signal(int sig, sighandler_t handler)
{
	sighandler_t old_handler;

	/* シグナル番号の有効性を検証 */
	if (!valid_signal(sig))
	{
		return SIG_ERR;
	}

	/* SIGKILLはハンドラ変更不可（強制終了を保証するため） */
	if (sig == SIGKILL)
	{
		return SIG_ERR;
	}

	/* 以前のハンドラを帰り値として保存する */
	old_handler = current->sig_actions[sig].sa_handler;

	/* 新しいハンドラを設定する */
	current->sig_actions[sig].sa_handler = handler;

	return old_handler;
}
