#include <asm-i386/ptrace.h>
#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/stddef.h>

/** entry.S が sys_sigreturn のために保存する ring-0 の pt_regs ポインタ
 * @note シングル CPU なのでグローバルで安全
 */
struct pt_regs *g_current_regs;

/** 現在実行中プロセス（kernel/sched/core.cで定義） */
extern struct task_struct *current;

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
 * @brief lib/unistd.c の同名関数。int $0x80 で sys_sigreturn を呼び元のコンテキストへ復帰する
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

	/* 各シグナルを順番にチェックして処理 */
	for (sig = 1; sig < _NSIG; sig++)
	{
		/* このシグナルが保留中でなければスキップ */
		if (!(current->pending.signal & (1UL << sig)))
		{
			continue;
		}

		/* 保留ビットをクリア（処理済みにする） */
		current->pending.signal &= ~(1UL << sig);
		handler = current->sig_actions[sig].sa_handler;

		/* SIG_IGN なら無視 */
		if (handler == SIG_IGN)
		{
			continue;
		}

		/* SIG_DFL ならデフォルト動作 */
		if (handler == SIG_DFL)
		{
			/* SIGKILL/SIGSEGV/SIGILL/SIGTERM/SIGFPE/SIGBUS など終了系はプロセスを終了 */
			if (sig == SIGKILL || sig == SIGSEGV || sig == SIGILL || sig == SIGTERM || sig == SIGFPE || sig == SIGBUS)
			{
				extern __attribute__((noreturn)) void do_exit(int code);
				/* 終了コードにシグナル番号を使う（POSIX 慣習） */
				do_exit(sig);
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
 * @brief sigreturn() から int $0x80 で呼ばれる（lib/unistd.c）
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

	/* 対象プロセスの保留シグナルビットマスクにセット */
	p->pending.signal |= (1UL << sig);

	return 0;
}

struct pgrp_signal_ctx
{
	pid_t pgrp;	   /* 対象プロセスグループID */
	int sig;	   /* 送信するシグナル番号 */
	int delivered; /* 送信したシグナルの数 */
	int error;	   /* 送信中にエラーが発生した場合は負のエラーコードをセット */
};

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
	ctx.delivered = 0;
	ctx.error = 0;

	if (task_for_each(kill_pg_cb, &ctx) < 0)
	{
		return -EINVAL;
	}
	if (ctx.error)
	{
		return ctx.error;
	}
	if (ctx.delivered == 0)
	{
		return -ESRCH;
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

	/* 送信先プロセスをPIDで検索 */
	p = find_task_by_pid(pid);
	if (p == (struct task_struct *)0)
	{
		return -ESRCH; /* プロセスが存在しない */
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
