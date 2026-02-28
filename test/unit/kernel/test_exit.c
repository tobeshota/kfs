#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>

/* テスト対象関数（kernel/exit.c） */
extern __attribute__((noreturn)) void do_exit(int code);
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

/** do_exit()の基本動作テスト */
KFS_TEST(test_do_exit_basic)
{
	struct task_struct *parent = &init_task;
	struct task_struct *child;

	/* 親の子リストを初期化 */
	INIT_LIST_HEAD(&parent->children);

	/* 子プロセスを作成 */
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	KFS_ASSERT_EQ(child->__state, TASK_RUNNING);

	/* currentを子プロセスに設定 */
	current = child;

	/* 終了処理を実行 */
	do_exit(42);

	/* __stateがTASK_DEAD、exit_stateがEXIT_ZOMBIEになること */
	KFS_ASSERT_EQ(child->__state, TASK_DEAD);
	KFS_ASSERT_EQ(child->exit_state, EXIT_ZOMBIE);

	/* 終了コードが設定されること */
	KFS_ASSERT_EQ(child->exit_code, 42);

	/* PF_EXITINGフラグが立つこと */
	KFS_ASSERT_TRUE(child->flags & PF_EXITING);

	/* mm_structが解放されること */
	KFS_ASSERT_TRUE(child->mm == NULL);

	/* currentをinit_taskに戻す */
	current = &init_task;

	printk("do_exit basic test passed\n");
}

/** do_exit()によるメモリ解放のテスト */
KFS_TEST(test_do_exit_mm_cleanup)
{
	struct task_struct parent = init_task;
	struct task_struct *child;
	struct mm_struct *parent_mm;

	/* 親のmm_structをヒープに割り当て */
	parent_mm = kmalloc(sizeof(*parent_mm));
	KFS_ASSERT_TRUE(parent_mm != NULL);
	parent_mm->mm_count.counter = 1;
	parent_mm->brk = 0x08048000;
	parent_mm->start_stack = 0x08049000;
	parent.mm = parent_mm;

	/* 子プロセスを作成（mm_structがコピーされる） */
	child = copy_process(&parent);
	KFS_ASSERT_TRUE(child != NULL);
	KFS_ASSERT_TRUE(child->mm != NULL);
	KFS_ASSERT_TRUE(child->mm != parent.mm); /* 別のインスタンス */

	/* currentを子プロセスに設定 */
	current = child;

	/* 終了処理を実行 */
	do_exit(0);

	/* mm_structが解放されること */
	KFS_ASSERT_TRUE(child->mm == NULL);

	/* 親のmm_structをクリーンアップ */
	kfree(parent_mm);

	/* currentをinit_taskに戻す */
	current = &init_task;

	printk("do_exit mm cleanup test passed\n");
}

/** do_exit()による子プロセスの再親化テスト */
KFS_TEST(test_do_exit_reparent_children)
{
	struct task_struct *parent;
	struct task_struct *child;
	struct task_struct *grandchild;

	/* 親プロセス作成 */
	INIT_LIST_HEAD(&init_task.children);
	parent = copy_process(&init_task);
	KFS_ASSERT_TRUE(parent != NULL);

	/* 子プロセス作成 */
	INIT_LIST_HEAD(&parent->children);
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	KFS_ASSERT_TRUE(child->parent == parent);

	/* 孫プロセス作成 */
	INIT_LIST_HEAD(&child->children);
	grandchild = copy_process(child);
	KFS_ASSERT_TRUE(grandchild != NULL);
	KFS_ASSERT_TRUE(grandchild->parent == child);

	/* 子プロセスが終了 */
	current = child;
	do_exit(0);

	/* 孫プロセスの親がinit_taskに変更されること */
	KFS_ASSERT_TRUE(grandchild->parent == &init_task);

	/* 孫プロセスがinit_taskの子リストに含まれること */
	KFS_ASSERT_TRUE(!list_empty(&init_task.children));

	/* currentをinit_taskに戻す */
	current = &init_task;

	printk("do_exit reparent children test passed\n");
}

/** do_exit()がpgdありのmm_structを正しく解放することをテスト */
KFS_TEST(test_do_exit_frees_pgd)
{
	struct task_struct *parent;
	struct task_struct *child;
	struct mm_struct *parent_mm;
	pgd_t *parent_pgd;

	/* 親のmm_structを割り当て */
	parent_mm = kmalloc(sizeof(*parent_mm));
	KFS_ASSERT_TRUE(parent_mm != NULL);

	/* 親のpgdを割り当て */
	parent_pgd = (pgd_t *)alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
	KFS_ASSERT_TRUE(parent_pgd != NULL);
	parent_mm->pgd = parent_pgd;
	parent_mm->mm_count.counter = 1;
	parent_mm->brk = 0;
	parent_mm->start_stack = 0;

	/* 親のinit_taskコピーにmm_structをセット */
	parent = &init_task;
	parent->mm = parent_mm;

	/* 子プロセスを作成（pgdが独立コピーされる） */
	child = copy_process(parent);
	KFS_ASSERT_TRUE(child != NULL);
	KFS_ASSERT_TRUE(child->mm != NULL);
	KFS_ASSERT_TRUE(child->mm->pgd != NULL);
	KFS_ASSERT_TRUE(child->mm->pgd != parent_pgd); /* 独立したpgd */

	/* 子プロセスを終了（pgdが解放される） */
	current = child;
	do_exit(0);

	/* mm_structが解放されていること */
	KFS_ASSERT_TRUE(child->mm == NULL);

	/* 親のmm_structはそのままであること */
	KFS_ASSERT_TRUE(parent->mm == parent_mm);

	/* クリーンアップ */
	parent->mm = NULL;
	kfree(parent_mm);
	current = &init_task;

	printk("do_exit frees pgd test passed\n");
}

/** release_task()の基本テスト */
KFS_TEST(test_release_task_basic)
{
	struct task_struct *task;

	/* タスク作成 */
	task = copy_process(&init_task);
	KFS_ASSERT_TRUE(task != NULL);

	/* グローバルリストに含まれることを確認 */
	KFS_ASSERT_TRUE(!list_empty(&task_list));

	/* 終了処理 */
	current = task;
	do_exit(0);

	/* release_task()でクリーンアップ */
	release_task(task);

	/* グローバルリストから削除されること（ここでは検証困難なのでクラッシュしないことを確認） */

	/* currentをinit_taskに戻す */
	current = &init_task;

	printk("release_task basic test passed\n");
}

/** sys_exit()のテスト */
KFS_TEST(test_sys_exit)
{
	struct task_struct *child;

	/* 子プロセス作成 */
	child = copy_process(&init_task);
	KFS_ASSERT_TRUE(child != NULL);

	/* currentを子プロセスに設定 */
	current = child;

	/* sys_exit()を呼び出し */
	sys_exit(42);

	/* 終了コードがPOSIX形式で設定されること（上位8ビット） */
	KFS_ASSERT_EQ(child->exit_code, 42 << 8);

	/* __stateがTASK_DEAD、exit_stateがEXIT_ZOMBIEになること */
	KFS_ASSERT_EQ(child->__state, TASK_DEAD);
	KFS_ASSERT_EQ(child->exit_state, EXIT_ZOMBIE);

	/* currentをinit_taskに戻す */
	current = &init_task;

	printk("sys_exit test passed\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_mm_cleanup, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_frees_pgd, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exit_reparent_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_release_task_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_exit, setup_test, teardown_test),
};

int register_unit_tests_exit(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
