#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>

static void init_fair_task(struct task_struct *task, pid_t pid, uint64_t vruntime)
{
	task->__state = TASK_RUNNING;
	task->pid = pid;
	task->policy = SCHED_NORMAL;
	task->time_slice = RR_TIMESLICE;
	INIT_LIST_HEAD(&task->run_list);
	task->se.load = 0;
	task->se.run_node.__rb_parent_color = 0;
	task->se.run_node.rb_left = NULL;
	task->se.run_node.rb_right = NULL;
	task->se.on_rq = 0;
	task->se.vruntime = vruntime;
}

static void setup_test(void)
{
	reset_all_state_for_test();
}

static void teardown_test(void)
{
}

/* SCHED_NORMAL task は fair runqueue に入り，最小 vruntime の task が選ばれる */
static void test_fair_pick_next_lowest_vruntime(void)
{
	struct task_struct a;
	struct task_struct b;
	struct task_struct c;

	init_fair_task(&a, 10, 30);
	init_fair_task(&b, 11, 10);
	init_fair_task(&c, 12, 20);

	sched_enqueue_task(&a);
	sched_enqueue_task(&b);
	sched_enqueue_task(&c);

	KFS_ASSERT_TRUE(sched_task_queued(&a));
	KFS_ASSERT_TRUE(sched_task_queued(&b));
	KFS_ASSERT_TRUE(sched_task_queued(&c));
	KFS_ASSERT_TRUE(sched_pick_next_task() == &b);
	KFS_ASSERT_TRUE(list_empty(&b.run_list));

	printk("fair: pick lowest vruntime OK\n");
}

/* fair dequeue は task を rbtree から外し，次の最小 vruntime task を選べる */
static void test_fair_dequeue_updates_leftmost(void)
{
	struct task_struct a;
	struct task_struct b;
	struct task_struct c;

	init_fair_task(&a, 20, 30);
	init_fair_task(&b, 21, 10);
	init_fair_task(&c, 22, 20);
	sched_enqueue_task(&a);
	sched_enqueue_task(&b);
	sched_enqueue_task(&c);

	sched_dequeue_task(&b);

	KFS_ASSERT_TRUE(!sched_task_queued(&b));
	KFS_ASSERT_TRUE(sched_pick_next_task() == &c);

	printk("fair: dequeue updates leftmost OK\n");
}

/* fair dequeue は左右に子を持つ node でも tree を保つ */
static void test_fair_dequeue_node_with_two_children(void)
{
	struct task_struct root;
	struct task_struct left;
	struct task_struct right;
	struct task_struct mid;

	init_fair_task(&root, 30, 20);
	init_fair_task(&left, 31, 10);
	init_fair_task(&right, 32, 30);
	init_fair_task(&mid, 33, 25);
	sched_enqueue_task(&root);
	sched_enqueue_task(&left);
	sched_enqueue_task(&right);
	sched_enqueue_task(&mid);

	sched_dequeue_task(&root);

	KFS_ASSERT_TRUE(!sched_task_queued(&root));
	KFS_ASSERT_TRUE(sched_pick_next_task() == &left);
	sched_dequeue_task(&left);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &mid);

	printk("fair: dequeue node with two children OK\n");
}

/* 同じ fair task を二重 enqueue しても runqueue 上では一件として扱う */
static void test_fair_enqueue_no_duplicate(void)
{
	struct task_struct task;

	init_fair_task(&task, 40, 5);
	sched_enqueue_task(&task);
	sched_enqueue_task(&task);

	KFS_ASSERT_TRUE(sched_pick_next_task() == &task);
	sched_dequeue_task(&task);
	KFS_ASSERT_TRUE(sched_pick_next_task() == 0);

	printk("fair: enqueue no duplicate OK\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_pick_next_lowest_vruntime, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_dequeue_updates_leftmost, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_dequeue_node_with_two_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_enqueue_no_duplicate, setup_test, teardown_test),
};

int register_unit_tests_fair(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
