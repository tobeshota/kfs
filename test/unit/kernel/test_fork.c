#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/string.h>

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
extern struct list_head task_list;

/** テスト専用：init_taskとtask_listを強制的にリセット
 * @note 各単体テスト前にグローバル状態をクリーンアップするために使用
 */
static void reset_init_task_for_test(void)
{
	/* task_listをクリア */
	INIT_LIST_HEAD(&task_list);

	/* init_taskのリストをリセット */
	INIT_LIST_HEAD(&init_task.children);
	INIT_LIST_HEAD(&init_task.sibling);
	INIT_LIST_HEAD(&init_task.tasks);

	/* init_taskを再初期化 */
	init_task.__state = TASK_RUNNING;
	init_task.pid = 0;
	init_task.parent = &init_task;

	/* currentをリセット */
	current = &init_task;
}

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();

	/* スラブアロケータ初期化 */
	kmem_cache_init();

	/* PID管理初期化 */
	pid_init();

	/* init_taskとtask_listを強制リセット */
	reset_init_task_for_test();

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

/** copy_process()のメモリコピーテスト
 * @note Phase 4で修正：copy_page_tables()によるページテーブルコピー
 */
KFS_TEST(test_copy_process_mm)
{
	struct task_struct parent;
	struct task_struct *child;
	struct mm_struct parent_mm = {0};

	/* 親task_structを安全に初期化（init_taskのコピーは危険） */
	memset(&parent, 0, sizeof(parent));
	parent.__state = TASK_RUNNING;
	parent.pid = 1;
	parent.flags = 0;
	INIT_LIST_HEAD(&parent.children);
	INIT_LIST_HEAD(&parent.sibling);
	INIT_LIST_HEAD(&parent.tasks);
	parent.signal = NULL;				  /* copy_signal()で新規割り当てされる */
	INIT_LIST_HEAD(&parent.pending.list); /* シグナルキューを初期化 */
	parent.pending.signal = 0;

	/* 親にmm_structを設定 */
	parent.mm = &parent_mm;
	parent_mm.mm_count.counter = 1;
	parent_mm.pgd = NULL; /* NULLでもcopy_mm()は動作する（カーネルスレッドとして処理） */
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
	struct task_struct parent;
	struct task_struct *child1, *child2;

	/* 親task_structを安全に初期化 */
	memset(&parent, 0, sizeof(parent));
	parent.__state = TASK_RUNNING;
	parent.pid = 1;
	parent.flags = 0;
	parent.mm = NULL; /* カーネルスレッドとして扱う */
	parent.signal = NULL;
	INIT_LIST_HEAD(&parent.children);
	INIT_LIST_HEAD(&parent.sibling);
	INIT_LIST_HEAD(&parent.tasks);
	INIT_LIST_HEAD(&parent.pending.list); /* シグナルキューを初期化 */
	parent.pending.signal = 0;

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

/** copy_process()でpgdありの親から独立したメモリ空間を持つ子が生成されることをテスト */
KFS_TEST(test_copy_process_independent_pgd)
{
	struct task_struct parent;
	struct task_struct *child;
	struct mm_struct parent_mm = {0};
	pgd_t *parent_pgd;

	/* 親task_structを初期化 */
	memset(&parent, 0, sizeof(parent));
	parent.__state = TASK_RUNNING;
	parent.pid = 1;
	parent.flags = 0;
	INIT_LIST_HEAD(&parent.children);
	INIT_LIST_HEAD(&parent.sibling);
	INIT_LIST_HEAD(&parent.tasks);
	parent.signal = NULL;
	INIT_LIST_HEAD(&parent.pending.list);
	parent.pending.signal = 0;

	/* 親にpgdありのmm_structを設定 */
	parent_pgd = (pgd_t *)alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
	KFS_ASSERT_TRUE(parent_pgd != NULL);
	parent_mm.pgd = parent_pgd;
	parent_mm.mm_count.counter = 1;
	parent.mm = &parent_mm;

	/* copy_process()でコピー */
	child = copy_process(&parent);
	KFS_ASSERT_TRUE(child != NULL);

	/* 子のmm_structが独立したインスタンスであること */
	KFS_ASSERT_TRUE(child->mm != NULL);
	KFS_ASSERT_TRUE(child->mm != parent.mm);

	/* 子のpgdが親と異なるポインタ（独立したコピー）であること */
	KFS_ASSERT_TRUE(child->mm->pgd != NULL);
	KFS_ASSERT_TRUE(child->mm->pgd != parent_pgd);

	/* 親のpgdはそのまま残っていること */
	KFS_ASSERT_TRUE(parent_mm.pgd == parent_pgd);

	printk("copy_process independent pgd test passed\n");
}

/** do_fork()の基本テスト */
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
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_process_independent_pgd, setup_test, teardown_test),
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
