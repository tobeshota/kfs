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
	task->prio = DEFAULT_PRIO;
	task->static_prio = DEFAULT_PRIO;
	task->nice = 0;
	task->time_slice = RR_TIMESLICE;
	INIT_LIST_HEAD(&task->run_list);
	task->se.vruntime = vruntime;
	sched_init_entity(task);
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

/* nice 0 は CFS の基準 weight になり、範囲外 nice は端に丸められる */
static void test_fair_weight_table_nice_zero_and_clamp(void)
{
	KFS_ASSERT_TRUE(sched_weight_for_nice(0) == NICE_0_LOAD);
	KFS_ASSERT_TRUE(sched_weight_for_nice(NICE_MIN) > NICE_0_LOAD);
	KFS_ASSERT_TRUE(sched_weight_for_nice(NICE_MAX) < NICE_0_LOAD);
	KFS_ASSERT_TRUE(sched_weight_for_nice(NICE_MIN - 1) == sched_weight_for_nice(NICE_MIN));
	KFS_ASSERT_TRUE(sched_weight_for_nice(NICE_MAX + 1) == sched_weight_for_nice(NICE_MAX));

	printk("fair: nice weight table OK\n");
}

/* fair tick は nice 0 task の vruntime を 1 tick 分進める */
static void test_fair_tick_updates_vruntime(void)
{
	struct task_struct task;

	init_fair_task(&task, 50, 0);
	sched_task_tick(&task);

	KFS_ASSERT_TRUE(task.se.vruntime == NICE_0_LOAD);

	printk("fair: tick updates vruntime OK\n");
}

/* nice weight が大きい task ほど同じ実行時間で増える vruntime は小さい */
static void test_fair_tick_scales_by_nice_weight(void)
{
	struct task_struct high;
	struct task_struct base;
	struct task_struct low;

	init_fair_task(&high, 60, 0);
	init_fair_task(&base, 61, 0);
	init_fair_task(&low, 62, 0);
	high.nice = NICE_MIN;
	base.nice = 0;
	low.nice = NICE_MAX;
	high.se.load = sched_weight_for_nice(high.nice);
	base.se.load = sched_weight_for_nice(base.nice);
	low.se.load = sched_weight_for_nice(low.nice);

	sched_task_tick(&high);
	sched_task_tick(&base);
	sched_task_tick(&low);

	KFS_ASSERT_TRUE(high.se.vruntime < base.se.vruntime);
	KFS_ASSERT_TRUE(low.se.vruntime > base.se.vruntime);

	printk("fair: tick scales by nice weight OK\n");
}

/* tick 後に vruntime が進んだ task は tree 内で正しい位置へ戻される */
static void test_fair_tick_reorders_runqueue(void)
{
	struct task_struct a;
	struct task_struct b;

	init_fair_task(&a, 70, 0);
	init_fair_task(&b, 71, 1500);
	sched_enqueue_task(&a);
	sched_enqueue_task(&b);

	KFS_ASSERT_TRUE(sched_pick_next_task() == &a);
	sched_task_tick(&a);
	sched_task_tick(&a);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &b);

	printk("fair: tick reorders runqueue OK\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_pick_next_lowest_vruntime, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_dequeue_updates_leftmost, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_dequeue_node_with_two_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_enqueue_no_duplicate, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_weight_table_nice_zero_and_clamp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_tick_updates_vruntime, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_tick_scales_by_nice_weight, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_fair_tick_reorders_runqueue, setup_test, teardown_test),
};

int register_unit_tests_fair(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
