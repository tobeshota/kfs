#include "unit_test_framework.h"
#include <kfs/list.h>
#include <kfs/sched.h>
#include <kfs/string.h>

/**
 * test_init_task_initialization - init_taskの初期化検証
 *
 * init_taskが正しく初期化されているか確認
 */
static void test_init_task_initialization(void)
{
	extern struct task_struct init_task;

	/* PID 0である */
	KFS_ASSERT_TRUE(init_task.pid == 0);

	/* TASK_RUNNING状態 */
	KFS_ASSERT_TRUE(init_task.__state == TASK_RUNNING);

	/* カーネルスレッドフラグが立っている */
	KFS_ASSERT_TRUE(init_task.flags & PF_KTHREAD);

	/* root権限を持つ */
	KFS_ASSERT_TRUE(init_task.uid.val == 0);
	KFS_ASSERT_TRUE(init_task.euid.val == 0);

	/* 全Capability有効 */
	KFS_ASSERT_TRUE(init_task.cap_effective.cap[0] == 0xffffffff);

	/* プロセス名が"swapper" */
	KFS_ASSERT_TRUE(strcmp(init_task.comm, "swapper") == 0);

	/* 自分自身が親 */
	KFS_ASSERT_TRUE(init_task.parent == &init_task);

	printk("init_task: PID=%d, comm='%s', state=%u, uid=%u\n", init_task.pid, init_task.comm, init_task.__state,
		   init_task.uid.val);
}

/**
 * test_list_operations - list_head操作のテスト
 *
 * 双方向リストの基本操作が正しく動作するか確認
 */
static void test_list_operations(void)
{
	struct list_head head;
	struct list_head node1, node2, node3;

	/* リスト初期化 */
	INIT_LIST_HEAD(&head);
	KFS_ASSERT_TRUE(list_empty(&head));

	/* ノード追加 */
	list_add(&node1, &head);
	KFS_ASSERT_TRUE(!list_empty(&head));
	KFS_ASSERT_TRUE(head.next == &node1);
	KFS_ASSERT_TRUE(node1.prev == &head);

	/* 複数ノード追加 */
	list_add(&node2, &head);
	list_add_tail(&node3, &head);

	/* 順序確認：head -> node2 -> node1 -> node3 -> head */
	KFS_ASSERT_TRUE(head.next == &node2);
	KFS_ASSERT_TRUE(node2.next == &node1);
	KFS_ASSERT_TRUE(node1.next == &node3);
	KFS_ASSERT_TRUE(node3.next == &head);

	/* ノード削除 */
	list_del(&node1);
	KFS_ASSERT_TRUE(node2.next == &node3);

	printk("list operations test passed\n");
}

/**
 * test_task_state_constants - タスク状態定数の検証
 *
 * タスク状態定数が正しく定義されているか確認
 */
static void test_task_state_constants(void)
{
	/* 各状態が異なる値を持つ */
	KFS_ASSERT_TRUE(TASK_RUNNING == 0);
	KFS_ASSERT_TRUE(TASK_INTERRUPTIBLE != TASK_RUNNING);
	KFS_ASSERT_TRUE(TASK_UNINTERRUPTIBLE != TASK_RUNNING);

	printk("Task state constants: RUNNING=%u, INTERRUPTIBLE=%u, "
		   "UNINTERRUPTIBLE=%u\n",
		   TASK_RUNNING, TASK_INTERRUPTIBLE, TASK_UNINTERRUPTIBLE);
}

/* テスト登録 */
int register_unit_tests_sched_core(struct kfs_test_case **out)
{
	static struct kfs_test_case cases[] = {
		KFS_REGISTER_TEST(test_init_task_initialization),
		KFS_REGISTER_TEST(test_list_operations),
		KFS_REGISTER_TEST(test_task_state_constants),
	};
	*out = cases;
	return sizeof(cases) / sizeof(cases[0]);
}
