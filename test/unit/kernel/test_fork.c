#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>

/* テスト対象関数（kernel/fork.c） */
extern struct task_struct *copy_process(struct task_struct *orig);
extern pid_t do_fork(void);
extern void fork_init(void);

/* テスト用ヘルパー（kernel/sched/core.c） */
extern struct task_struct *find_task_by_pid(pid_t pid);
extern struct task_struct *current;
extern struct task_struct init_task;

/* 初期化関数（PIDとスラブアロケータ） */
extern void pid_init(void);
extern void init_idle_task(void);

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();

	/* スラブアロケータ初期化 */
	kmem_cache_init();

	/* PID管理初期化 */
	pid_init();

	/* init_task初期化 */
	init_idle_task();

	/* fork初期化 */
	fork_init();
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 必要なら後処理（現在は空） */
}

/** copy_process()の基本動作テスト */
KFS_TEST(test_copy_process_basic)
{
	struct task_struct *child;

	/* copy_process()でinit_taskをコピー */
	child = copy_process(&init_task);

	/* 子プロセスが作成されること */
	KFS_ASSERT_TRUE(child != NULL);

	/* 親がinit_taskであること */
	KFS_ASSERT_TRUE(child->parent == &init_task);

	/* 新しいPIDが割り当てられること */
	KFS_ASSERT_TRUE(child->pid != init_task.pid);

	/* 実行可能状態であること */
	KFS_ASSERT_EQ(child->__state, TASK_RUNNING);

	printk("copy_process basic test passed\n");
}

/** copy_process()のメモリコピーテスト */
KFS_TEST(test_copy_process_mm)
{
	struct task_struct parent = init_task;
	struct task_struct *child;
	struct mm_struct parent_mm = {0};

	/* 親にmm_structを設定 */
	parent.mm = &parent_mm;
	parent_mm.brk = 0x08048000;
	parent_mm.start_stack = 0x08049000;

	/* copy_process()でコピー */
	child = copy_process(&parent);
	KFS_ASSERT_TRUE(child != NULL);

	/* mm_structがコピーされること */
	KFS_ASSERT_TRUE(child->mm != NULL);
	KFS_ASSERT_TRUE(child->mm != parent.mm); /* 別のインスタンス */

	/* mm_structの内容がコピーされること */
	KFS_ASSERT_EQ(child->mm->brk, parent_mm.brk);
	KFS_ASSERT_EQ(child->mm->start_stack, parent_mm.start_stack);

	printk("copy_process mm test passed\n");
}

/** copy_process()の親子関係テスト */
KFS_TEST(test_copy_process_parent_child)
{
	struct task_struct parent = init_task;
	struct task_struct *child1, *child2;

	/* 親の子リストを初期化 */
	INIT_LIST_HEAD(&parent.children);

	/* 1つ目の子を作成 */
	child1 = copy_process(&parent);
	KFS_ASSERT_TRUE(child1 != NULL);

	/* 親の子リストに追加されること */
	KFS_ASSERT_TRUE(!list_empty(&parent.children));

	/* 2つ目の子を作成 */
	child2 = copy_process(&parent);
	KFS_ASSERT_TRUE(child2 != NULL);

	/* 両方とも親がparentであること */
	KFS_ASSERT_TRUE(child1->parent == &parent);
	KFS_ASSERT_TRUE(child2->parent == &parent);

	printk("copy_process parent-child test passed\n");
}

/** find_task_by_pid()の基本テスト */
KFS_TEST(test_find_task_by_pid_basic)
{
	struct task_struct *task;

	/* init_task（PID=0）が見つかること */
	task = find_task_by_pid(0);
	KFS_ASSERT_TRUE(task != NULL);
	KFS_ASSERT_EQ(task->pid, 0);

	printk("find_task_by_pid basic test passed\n");
}

/** find_task_by_pid()で存在しないPIDを検索 */
KFS_TEST(test_find_task_by_pid_not_found)
{
	struct task_struct *task;

	/* 存在しないPIDを検索 */
	task = find_task_by_pid(9999);

	/* NULLが返ること */
	KFS_ASSERT_TRUE(task == NULL);

	printk("find_task_by_pid not found test passed\n");
}

/** do_fork()の基本テスト */
KFS_TEST(test_do_fork_basic)
{
	pid_t child_pid;
	struct task_struct *child;

	/* currentをinit_taskに設定 */
	current = &init_task;

	/* do_fork()を実行 */
	child_pid = do_fork();

	/* 正のPIDが返ること */
	KFS_ASSERT_TRUE(child_pid > 0);

	/* 子プロセスが検索できること */
	child = find_task_by_pid(child_pid);
	KFS_ASSERT_TRUE(child != NULL);

	/* 子の親がcurrentであること */
	KFS_ASSERT_TRUE(child->parent == current);

	printk("do_fork basic test passed\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_process_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_process_mm, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_process_parent_child, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_find_task_by_pid_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_find_task_by_pid_not_found, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_fork_basic, setup_test, teardown_test),
};

int register_unit_tests_fork(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
