#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/wait.h>

/* テスト対象関数（kernel/wait.c） */
extern pid_t do_wait(int *wstatus);
extern pid_t sys_wait(int *wstatus);

/* 依存関数（kernel/exit.c） */
extern void do_exit(int code);
extern void release_task(struct task_struct *p);
extern void sys_exit(int error_code);

/* テスト用ヘルパー（kernel/sched/core.c） */
extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;

/* 初期化関数（kernel/fork.c） */
extern void fork_init(void);
extern struct task_struct *copy_process(struct task_struct *orig);

/* 初期化関数（PIDとスラブアロケータ） */
extern void pid_init(void);
extern void init_idle_task(void);

/** テスト専用：init_taskとtask_listを強制的にリセット */
static void reset_init_task_for_test(void)
{
	INIT_LIST_HEAD(&task_list);
	INIT_LIST_HEAD(&init_task.children);
	INIT_LIST_HEAD(&init_task.sibling);
	INIT_LIST_HEAD(&init_task.tasks);
	init_task.__state = TASK_RUNNING;
	init_task.pid = 0;
	init_task.parent = &init_task;
	current = &init_task;
}

static void setup_test(void)
{
	reset_all_state_for_test();
	kmem_cache_init();
	pid_init();
	reset_init_task_for_test();
	init_idle_task();
	fork_init();
}

static void teardown_test(void)
{
}

/** do_wait(): 子プロセスが存在しない場合は -ECHILD を返す */
KFS_TEST(test_do_wait_no_children)
{
	int status = 0;
	pid_t ret;

	/* init_task には子がいない状態 */
	INIT_LIST_HEAD(&init_task.children);
	current = &init_task;

	ret = do_wait(&status);

	KFS_ASSERT_EQ((int)ret, -ECHILD);
	printk("do_wait no children test passed\n");
}

/** do_wait(): EXIT_ZOMBIE の子（ゾンビ）を正しく回収する */
KFS_TEST(test_do_wait_zombie_child)
{
	struct task_struct *parent = &init_task;
	struct task_struct *child;
	int status = 0;
	pid_t ret;
	pid_t child_pid;

	/* 子プロセス作成 */
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	child_pid = child->pid;

	/* 子プロセスを終了させてゾンビ化 */
	current = child;
	do_exit(42);

	/* 親として wait */
	current = parent;
	ret = do_wait(&status);

	/* 子の PID が返ること */
	KFS_ASSERT_EQ((int)ret, (int)child_pid);

	/* 終了コードが正しく返ること（do_exit(42)はそのままexit_code=42） */
	KFS_ASSERT_EQ(status, 42);

	printk("do_wait zombie child test passed\n");
}

/** do_wait(): 実行中（TASK_RUNNING）の子しかいない場合は -EAGAIN を返す */
KFS_TEST(test_do_wait_running_child)
{
	struct task_struct *parent = &init_task;
	struct task_struct *child;
	int status = 0;
	pid_t ret;

	/* 子プロセス作成（TASK_RUNNING のまま） */
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	KFS_ASSERT_EQ((int)child->__state, (int)TASK_RUNNING);

	/* 親として wait → ゾンビがいないので -EAGAIN */
	current = parent;
	ret = do_wait(&status);

	KFS_ASSERT_EQ((int)ret, -EAGAIN);

	/* クリーンアップ（release_task を直接呼ぶ） */
	release_task(child);
	current = &init_task;

	printk("do_wait running child test passed\n");
}

/** do_wait(): wstatus に NULL を渡しても安全に動作する */
KFS_TEST(test_do_wait_null_wstatus)
{
	struct task_struct *parent = &init_task;
	struct task_struct *child;
	pid_t ret;
	pid_t child_pid;

	/* 子プロセス作成して終了 */
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	child_pid = child->pid;

	current = child;
	do_exit(1);

	/* wstatus = NULL でも segfault しないこと */
	current = parent;
	ret = do_wait(NULL);

	KFS_ASSERT_EQ((int)ret, (int)child_pid);

	printk("do_wait null wstatus test passed\n");
}

/** sys_wait(): do_wait() の薄いラッパーとして正しく動作する */
KFS_TEST(test_sys_wait_basic)
{
	struct task_struct *parent = &init_task;
	struct task_struct *child;
	int status = 0;
	pid_t ret;
	pid_t child_pid;

	/* 子プロセス作成して終了（sys_exit経由でPOSIXシフトされる） */
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	child_pid = child->pid;

	current = child;
	sys_exit(7); /* do_exit((7 & 0xff) << 8) が呼ばれる */

	/* sys_wait() で回収 */
	current = parent;
	ret = sys_wait(&status);

	KFS_ASSERT_EQ((int)ret, (int)child_pid);
	KFS_ASSERT_EQ(status, 7 << 8);

	printk("sys_wait basic test passed\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_no_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_zombie_child, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_running_child, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_wait_null_wstatus, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_wait_basic, setup_test, teardown_test),
};

int register_unit_tests_wait(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
