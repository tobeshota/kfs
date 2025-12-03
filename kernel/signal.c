#include <kfs/signal.h>
#include <kfs/stddef.h>

/** シグナルアクションテーブル
 * @brief 各シグナルのハンドラを保持する
 */
static struct sigaction sig_actions[_NSIG];

/** 保留中シグナルのビットマスク
 * @brief 発生したがまだ処理されていないシグナル
 * @note マルチプロセス環境ではpending_signalsをプロセスごとに保有する
 */
static unsigned long pending_signals;

/** シグナル番号が有効範囲内かを検証する
 * @param sig 検証するシグナル番号
 * @return 有効なら1，無効なら0
 */
static int valid_signal(int sig)
{
	/* シグナル番号は1からNSIG-1まで有効 */
	return sig > 0 && sig < _NSIG;
}

/** シグナルハンドラを登録する
 * @param sig シグナル番号
 * @param handler 登録するハンドラ関数
 * @return 以前のハンドラ，エラー時はSIG_ERR
 */
sighandler_t signal(int sig, sighandler_t handler)
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
	old_handler = sig_actions[sig].sa_handler;

	/* 新しいハンドラを設定する */
	sig_actions[sig].sa_handler = handler;

	return old_handler;
}

/** シグナルを発生させる
 * @brief シグナルを保留キューに追加する
 * @param sig 発生させるシグナル番号
 * @return 成功時は0、エラー時は-1
 */
int raise(int sig)
{
	/* シグナル番号の有効性を検証 */
	if (!valid_signal(sig))
	{
		return -1;
	}

	/* 保留シグナルビットマスクに該当シグナルをセット */
	pending_signals |= (1UL << sig);

	return 0;
}

/** 保留中シグナルを処理する
 * @brief カーネル内部で呼び出され，登録済みハンドラを実行する
 */
void do_signal(void)
{
	int sig;
	sighandler_t handler;

	/* 保留シグナルがなければ何もしない */
	if (pending_signals == 0)
	{
		return;
	}

	/* 各シグナルを順番にチェックして処理 */
	for (sig = 1; sig < _NSIG; sig++)
	{
		/* このシグナルが保留中でなければスキップ */
		if (!(pending_signals & (1UL << sig)))
		{
			continue;
		}

		/* 保留ビットをクリア（処理済みにする） */
		pending_signals &= ~(1UL << sig);

		handler = sig_actions[sig].sa_handler;

		/* SIG_IGNなら無視 */
		if (handler == SIG_IGN)
		{
			continue;
		}

		/* SIG_DFLならデフォルト動作 */
		if (handler == SIG_DFL)
		{
			/* シグナル送信対象(プロセス)を定義したとき，シグナルごとのデフォルト動作を実装する */
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
	return pending_signals != 0;
}
