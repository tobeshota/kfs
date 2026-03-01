#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/stddef.h>

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

/** 保留中シグナルを処理する
 * @brief カーネル内部で呼び出され，登録済みハンドラを実行する
 */
void do_signal(void)
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

		/* SIG_IGNなら無視 */
		if (handler == SIG_IGN)
		{
			continue;
		}

		/* SIG_DFLならデフォルト動作 */
		if (handler == SIG_DFL)
		{
			/* SIGKILL/SIGSEGV/SIGILL/SIGTERM/SIGFPE/SIGBUS など終了系はプロセスを終了 */
			if (sig == SIGKILL || sig == SIGSEGV || sig == SIGILL || sig == SIGTERM || sig == SIGFPE || sig == SIGBUS)
			{
				extern __attribute__((noreturn)) void do_exit(int code);
				/* 終了コードにシグナル番号を使う（POSIX慣習） */
				do_exit(sig);
			}
			continue;
		}

		/* ユーザー定義ハンドラを呼び出す */
		handler(sig);
	}
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
