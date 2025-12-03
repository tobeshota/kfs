/**
 * @file test_signal.c
 * @brief シグナル管理の単体テスト
 */

#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/signal.h>

/* テスト用のハンドラ呼び出し記録 */
static int handler_called;
static int handler_received_sig;

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
	handler_called = 0;
	handler_received_sig = 0;
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* シグナルハンドラをデフォルトにリセット */
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGUSR1, SIG_DFL);
}

/* テスト用シグナルハンドラ */
static void test_handler(int sig)
{
	handler_called = 1;
	handler_received_sig = sig;
}

/* 別のテスト用シグナルハンドラ（カウンタ増加） */
static int handler_count;
static void counting_handler(int sig)
{
	(void)sig;
	handler_count++;
}

/* signal()で有効なシグナルにハンドラを登録できることをテスト */
KFS_TEST(test_signal_register_handler)
{
	sighandler_t old;

	/* SIGINTにハンドラを登録 */
	old = signal(SIGINT, test_handler);
	/* 初期状態はSIG_DFL */
	KFS_ASSERT_EQ((long)SIG_DFL, (long)old);

	/* 再度登録すると以前のハンドラが返る */
	old = signal(SIGINT, SIG_IGN);
	KFS_ASSERT_EQ((long)test_handler, (long)old);
}

/* signal()で無効なシグナル番号を拒否することをテスト */
KFS_TEST(test_signal_invalid_signum)
{
	sighandler_t result;

	/* シグナル番号0は無効 */
	result = signal(0, test_handler);
	KFS_ASSERT_EQ((long)SIG_ERR, (long)result);

	/* 負のシグナル番号は無効 */
	result = signal(-1, test_handler);
	KFS_ASSERT_EQ((long)SIG_ERR, (long)result);

	/* 範囲外のシグナル番号は無効 */
	result = signal(_NSIG, test_handler);
	KFS_ASSERT_EQ((long)SIG_ERR, (long)result);
}

/* signal()でSIGKILLのハンドラ変更を拒否することをテスト */
KFS_TEST(test_signal_sigkill_immutable)
{
	sighandler_t result;

	/* SIGKILLはハンドラ変更不可 */
	result = signal(SIGKILL, test_handler);
	KFS_ASSERT_EQ((long)SIG_ERR, (long)result);

	/* SIG_IGNも設定不可 */
	result = signal(SIGKILL, SIG_IGN);
	KFS_ASSERT_EQ((long)SIG_ERR, (long)result);
}

/* raise()でシグナルを発生させることをテスト */
KFS_TEST(test_raise_valid_signal)
{
	int ret;

	/* 有効なシグナルを発生 */
	ret = raise(SIGINT);
	KFS_ASSERT_EQ(0, ret);

	/* シグナルが保留中になる */
	KFS_ASSERT_EQ(1, signal_pending());
}

/* raise()で無効なシグナル番号を拒否することをテスト */
KFS_TEST(test_raise_invalid_signal)
{
	int ret;

	/* シグナル番号0は無効 */
	ret = raise(0);
	KFS_ASSERT_EQ(-1, ret);

	/* 負のシグナル番号は無効 */
	ret = raise(-1);
	KFS_ASSERT_EQ(-1, ret);

	/* 範囲外のシグナル番号は無効 */
	ret = raise(_NSIG);
	KFS_ASSERT_EQ(-1, ret);
}

/* do_signal()が登録済みハンドラを呼び出すことをテスト */
KFS_TEST(test_do_signal_calls_handler)
{
	/* ハンドラを登録 */
	signal(SIGINT, test_handler);

	/* シグナルを発生 */
	raise(SIGINT);

	/* まだハンドラは呼ばれていない */
	KFS_ASSERT_EQ(0, handler_called);

	/* do_signal()でハンドラを実行 */
	do_signal();

	/* ハンドラが呼ばれたことを確認 */
	KFS_ASSERT_EQ(1, handler_called);
	KFS_ASSERT_EQ(SIGINT, handler_received_sig);

	/* シグナルが処理済みになる */
	KFS_ASSERT_EQ(0, signal_pending());
}

/* do_signal()がSIG_IGNを無視することをテスト */
KFS_TEST(test_do_signal_ignores_sig_ign)
{
	/* SIG_IGNを設定 */
	signal(SIGTERM, SIG_IGN);

	/* シグナルを発生 */
	raise(SIGTERM);
	KFS_ASSERT_EQ(1, signal_pending());

	/* do_signal()を呼ぶとシグナルは無視される */
	do_signal();

	/* シグナルが処理済みになる（無視された） */
	KFS_ASSERT_EQ(0, signal_pending());
}

/* do_signal()がSIG_DFLを処理することをテスト */
KFS_TEST(test_do_signal_handles_sig_dfl)
{
	/* デフォルトハンドラのまま */
	signal(SIGUSR1, SIG_DFL);

	/* シグナルを発生 */
	raise(SIGUSR1);
	KFS_ASSERT_EQ(1, signal_pending());

	/* do_signal()を呼ぶとデフォルト動作が実行される */
	do_signal();

	/* シグナルが処理済みになる */
	KFS_ASSERT_EQ(0, signal_pending());
}

/* 複数のシグナルを同時に処理できることをテスト */
KFS_TEST(test_do_signal_multiple_signals)
{
	handler_count = 0;

	/* 複数のシグナルにハンドラを登録 */
	signal(SIGINT, counting_handler);
	signal(SIGTERM, counting_handler);
	signal(SIGUSR1, counting_handler);

	/* 複数のシグナルを発生 */
	raise(SIGINT);
	raise(SIGTERM);
	raise(SIGUSR1);

	/* do_signal()で全て処理 */
	do_signal();

	/* 3つのハンドラが呼ばれたことを確認 */
	KFS_ASSERT_EQ(3, handler_count);
	KFS_ASSERT_EQ(0, signal_pending());
}

/* signal_pending()が保留シグナルを正しく報告することをテスト */
KFS_TEST(test_signal_pending_reports_correctly)
{
	/* 初期状態では保留なし */
	KFS_ASSERT_EQ(0, signal_pending());

	/* シグナルを発生させると保留あり */
	raise(SIGINT);
	KFS_ASSERT_EQ(1, signal_pending());

	/* 処理すると保留なし */
	signal(SIGINT, SIG_IGN);
	do_signal();
	KFS_ASSERT_EQ(0, signal_pending());
}

/* テスト登録 */
static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_signal_register_handler, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_signal_invalid_signum, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_signal_sigkill_immutable, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_raise_valid_signal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_raise_invalid_signal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_signal_calls_handler, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_signal_ignores_sig_ign, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_signal_handles_sig_dfl, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_signal_multiple_signals, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_signal_pending_reports_correctly, setup_test, teardown_test),
};

int register_unit_tests_signal(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
