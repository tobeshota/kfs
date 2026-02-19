/**
 * @file test_signal.c
 * @brief シグナル管理の単体テスト
 */

#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/signal.h>

/* current、init_task、task_list は kernel/sched/core.c で定義 */
extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;
extern void init_idle_task(void);

/* テスト用のハンドラ呼び出し記録 */
static int handler_called;
static int handler_received_sig;

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	int i;

	reset_all_state_for_test();

	/* task_listとinit_taskのリストをリセット（sys_kill用） */
	INIT_LIST_HEAD(&task_list);
	INIT_LIST_HEAD(&init_task.tasks);
	init_task.pid = 0;
	init_task.parent = &init_task;

	/* init_taskをタスクリストに追加 */
	init_idle_task();

	/* currentをinit_taskにリセット */
	current = &init_task;

	/* 保留シグナルをクリア */
	current->pending.signal = 0;

	/* シグナルアクションテーブルをSIG_DFLにリセット */
	for (i = 0; i < _NSIG; i++)
	{
		current->sig_actions[i].sa_handler = SIG_DFL;
		current->sig_actions[i].sa_flags = 0;
	}

	handler_called = 0;
	handler_received_sig = 0;
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 保留シグナルをクリア */
	current->pending.signal = 0;

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

/* send_signal()が対象プロセスの保留シグナルをセットすることをテスト */
KFS_TEST(test_send_signal_sets_pending)
{
	struct task_struct target;
	int i;
	int ret;

	/* ターゲットプロセスを初期化 */
	target.pending.signal = 0;
	for (i = 0; i < _NSIG; i++)
	{
		target.sig_actions[i].sa_handler = SIG_DFL;
		target.sig_actions[i].sa_flags = 0;
	}

	/* currentには保留シグナルなし */
	KFS_ASSERT_EQ(0, signal_pending());

	/* targetにシグナルを送信 */
	ret = send_signal(SIGUSR1, &target);
	KFS_ASSERT_EQ(0, ret);

	/* targetにシグナルが保留中になる */
	KFS_ASSERT_TRUE(target.pending.signal & (1UL << SIGUSR1));

	/* currentには影響しない */
	KFS_ASSERT_EQ(0, signal_pending());
}

/* send_signal()がNULLプロセスを拒否することをテスト */
KFS_TEST(test_send_signal_null_process)
{
	int ret;

	ret = send_signal(SIGINT, (struct task_struct *)0);
	KFS_ASSERT_EQ(-1, ret);
}

/* send_signal()が無効なシグナル番号を拒否することをテスト */
KFS_TEST(test_send_signal_invalid_signum)
{
	int ret;

	ret = send_signal(0, current);
	KFS_ASSERT_EQ(-1, ret);

	ret = send_signal(-1, current);
	KFS_ASSERT_EQ(-1, ret);

	ret = send_signal(_NSIG, current);
	KFS_ASSERT_EQ(-1, ret);
}

/* sys_kill()が有効なPIDにシグナルを送信することをテスト */
KFS_TEST(test_sys_kill_valid_pid)
{
	int ret;

	/* init_task (PID 0) へSIGINTを送信 */
	ret = sys_kill(0, SIGINT);
	KFS_ASSERT_EQ(0, ret);

	/* init_taskの保留シグナルにSIGINTがセットされる */
	KFS_ASSERT_TRUE(current->pending.signal & (1UL << SIGINT));
}

/* sys_kill()が存在しないPIDを拒否することをテスト */
KFS_TEST(test_sys_kill_invalid_pid)
{
	int ret;

	/* 存在しないPIDへのシグナル送信はエラー */
	ret = sys_kill(9999, SIGINT);
	KFS_ASSERT_EQ(-ESRCH, ret);
}

/* sys_signal()がsignal()と同等に動作することをテスト */
KFS_TEST(test_sys_signal_same_as_signal)
{
	sighandler_t old;

	/* sys_signal()でハンドラを登録 */
	old = sys_signal(SIGINT, test_handler);
	KFS_ASSERT_EQ((long)SIG_DFL, (long)old);

	/* 登録したハンドラが有効 */
	raise(SIGINT);
	do_signal();
	KFS_ASSERT_EQ(1, handler_called);
	KFS_ASSERT_EQ(SIGINT, handler_received_sig);
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
	KFS_REGISTER_TEST_WITH_SETUP(test_send_signal_sets_pending, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_send_signal_null_process, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_send_signal_invalid_signum, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_kill_valid_pid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_kill_invalid_pid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_signal_same_as_signal, setup_test, teardown_test),
};

int register_unit_tests_signal(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
