#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/slab.h>
#include <kfs/wait.h>

/**
 * test_wait.c — do_wait / sys_wait の単体テスト
 *
 * 【テスト方針】
 * test_exit.c と同様に、kernel_thread() + do_wait() を使う。
 * 子プロセスは kernel_thread でカーネルスレッドとして生成し（ring-0 実行）、
 * do_wait が schedule() 経由で子を実行させてゾンビを回収する。
 * do_fork(user_eip, user_esp) はカーネル関数ポインタを渡すと ring-3 iret 経路に
 * なり ESP が崩壊するため使用しない。
 */

/* テスト対象関数（kernel/wait.c） */
extern pid_t do_wait(int *wstatus, int options);
extern pid_t sys_wait(int *wstatus);

/* 依存関数（kernel/exit.c） */
extern void sys_exit(int error_code);

/* テスト用ヘルパー（kernel/sched/core.c） */
extern struct task_struct *current;
extern struct task_struct init_task;

/* 初期化関数 */
extern void fork_init(void);
extern pid_t kernel_thread(void (*fn)(void), const char *name);
extern void pid_init(void);
extern void init_idle_task(void);

static void setup_test(void)
{
	reset_all_state_for_test();
	kmem_cache_init();
	pid_init();
	init_idle_task();
	fork_init();
}

static void teardown_test(void)
{
}

/* ---- 子プロセス用ヘルパー関数 ---- */

static void fn_exit_0(void)
{
	sys_exit(0);
}

static void fn_exit_42(void)
{
	sys_exit(42);
}

static void fn_sys_exit_7(void)
{
	sys_exit(7);
}

/** do_wait(): 子プロセスが存在しない場合は -ECHILD を返す */
KFS_TEST(test_do_wait_no_children)
{
	int status = 0;
	pid_t ret;

	ret = do_wait(&status, 0);

	KFS_ASSERT_EQ((int)ret, -ECHILD);
	printk("do_wait no children test passed\n");
}

/** do_wait(): ゾンビ化した子を正しく回収し、終了コードを返す */
KFS_TEST(test_do_wait_zombie_child)
{
	int status = 0;
	pid_t child_pid, ret;

	child_pid = kernel_thread(fn_exit_42, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);

	/* do_wait が schedule() で子を実行させ、子がゾンビになった後に回収 */
	ret = do_wait(&status, 0);

	KFS_ASSERT_EQ((int)ret, (int)child_pid);
	/* sys_exit(42) → do_exit((42 & 0xff) << 8) → wstatus = 42 << 8 */
	KFS_ASSERT_EQ(status, 42 << 8);

	printk("do_wait zombie child test passed\n");
}

/** do_wait(): wstatus に NULL を渡しても安全に動作する */
KFS_TEST(test_do_wait_null_wstatus)
{
	pid_t child_pid, ret;

	child_pid = kernel_thread(fn_exit_0, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);

	/* wstatus = NULL でも segfault しないこと */
	ret = do_wait(NULL, 0);

	KFS_ASSERT_EQ((int)ret, (int)child_pid);

	printk("do_wait null wstatus test passed\n");
}

/** sys_wait(): do_wait() の薄いラッパーとして正しく動作する */
KFS_TEST(test_sys_wait_basic)
{
	int status = 0;
	pid_t child_pid, ret;

	/* sys_exit(7) → do_exit((7 & 0xff) << 8) → wstatus = 7 << 8 */
	child_pid = kernel_thread(fn_sys_exit_7, NULL);
	KFS_ASSERT_TRUE(child_pid > 0);

	ret = sys_wait(&status);

	KFS_ASSERT_EQ((int)ret, (int)child_pid);
	KFS_ASSERT_EQ(status, 7 << 8);

	printk("sys_wait basic test passed\n");
}

KFS_TEST(test_do_waitpid_wuntraced_reports_stopped)
{
	struct task_struct child = init_task;
	int status = 0;
	pid_t ret;

	child.pid = 42;
	child.parent = current;
	child.exit_state = 0;
	child.__state = __TASK_STOPPED;
	child.flags |= PF_WAIT_STOP_PENDING;
	child.exit_signal = SIGTSTP;
	INIT_LIST_HEAD(&child.children);
	INIT_LIST_HEAD(&child.sibling);
	INIT_LIST_HEAD(&child.run_list);
	list_add_tail(&child.sibling, &current->children);

	ret = do_waitpid(42, &status, WNOHANG | WUNTRACED);

	KFS_ASSERT_EQ((int)ret, 42);
	KFS_ASSERT_TRUE(WIFSTOPPED(status));
	KFS_ASSERT_EQ(WSTOPSIG(status), SIGTSTP);
	KFS_ASSERT_TRUE((child.flags & PF_WAIT_STOP_PENDING) == 0);

	list_del(&child.sibling);
}

KFS_TEST(test_do_waitpid_wcontinued_reports_resumed)
{
	struct task_struct child = init_task;
	int status = 0;
	pid_t ret;

	child.pid = 43;
	child.parent = current;
	child.exit_state = 0;
	child.__state = TASK_RUNNING;
	child.flags |= PF_WAIT_CONT_PENDING;
	INIT_LIST_HEAD(&child.children);
	INIT_LIST_HEAD(&child.sibling);
	INIT_LIST_HEAD(&child.run_list);
	list_add_tail(&child.sibling, &current->children);

	ret = do_waitpid(43, &status, WNOHANG | WCONTINUED);

	KFS_ASSERT_EQ((int)ret, 43);
	KFS_ASSERT_TRUE(WIFCONTINUED(status));
	KFS_ASSERT_TRUE((child.flags & PF_WAIT_CONT_PENDING) == 0);

	list_del(&child.sibling);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_no_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_zombie_child, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_null_wstatus, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_wait_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_waitpid_wuntraced_reports_stopped, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_waitpid_wcontinued_reports_resumed, setup_test, teardown_test),
};

int register_unit_tests_wait(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
