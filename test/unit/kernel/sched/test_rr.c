#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>

extern struct task_struct init_task;

/* ------------------------------------------------------------------ */
/* ヘルパ                                                               */
/* ------------------------------------------------------------------ */

/* テスト用タスクを最小限に初期化する */
static void init_test_task(struct task_struct *tsk, unsigned int time_slice)
{
	tsk->__state = TASK_RUNNING;
	tsk->policy = SCHED_PURE_RR;
	tsk->time_slice = time_slice;
	INIT_LIST_HEAD(&tsk->run_list);
}

/* ------------------------------------------------------------------ */
/* setup / teardown                                                     */
/* ------------------------------------------------------------------ */

static void setup_test(void)
{
	reset_all_state_for_test();
	rr_init(); /* RR ランキューをリセット */

	/* init_task.run_list が前テストの残骸を持たないよう初期化 */
	INIT_LIST_HEAD(&init_task.run_list);
	init_task.time_slice = RR_TIMESLICE;
	current = &init_task;
}

static void teardown_test(void)
{
	/* 後処理は現在不要 */
}

/* ------------------------------------------------------------------ */
/* テスト: rr_enqueue                                                   */
/* ------------------------------------------------------------------ */

/* rr_enqueue() でタスクがランキューに追加されることを確かめる */
static void test_rr_enqueue_adds_task(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	KFS_ASSERT_TRUE(list_empty(&tsk.run_list)); /* 追加前は空 */

	rr_enqueue(&tsk);

	KFS_ASSERT_TRUE(!list_empty(&tsk.run_list)); /* ノードがリンクされた */
	KFS_ASSERT_TRUE(rr_pick_next() == &tsk);	 /* キュー先頭に現れる */

	printk("rr_enqueue: task added to runqueue OK\n");
}

/* 同じタスクを二重登録しても一回分しか追加されないことを確かめる */
static void test_rr_enqueue_no_duplicate(void)
{
	struct task_struct tsk;
	struct task_struct tsk2;

	init_test_task(&tsk, RR_TIMESLICE);
	init_test_task(&tsk2, RR_TIMESLICE);

	rr_enqueue(&tsk);
	rr_enqueue(&tsk); /* 二重登録は無視されるべき */
	rr_enqueue(&tsk2);

	/* tsk → tsk2 の順（tsk が一つだけ） */
	KFS_ASSERT_TRUE(rr_pick_next() == &tsk);
	rr_dequeue(&tsk);
	KFS_ASSERT_TRUE(rr_pick_next() == &tsk2);
	rr_dequeue(&tsk2);
	KFS_ASSERT_TRUE(rr_pick_next() == 0); /* 空 */

	printk("rr_enqueue: no duplicate insertion OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト: rr_dequeue                                                   */
/* ------------------------------------------------------------------ */

/* rr_dequeue() でタスクがランキューから削除されることを確かめる */
static void test_rr_dequeue_removes_task(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	rr_enqueue(&tsk);
	KFS_ASSERT_TRUE(!list_empty(&tsk.run_list)); /* 登録確認 */

	rr_dequeue(&tsk);

	KFS_ASSERT_TRUE(list_empty(&tsk.run_list)); /* ノードが切り離された */
	KFS_ASSERT_TRUE(rr_pick_next() == 0);		/* キューが空 */

	printk("rr_dequeue: task removed from runqueue OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト: rr_pick_next                                                 */
/* ------------------------------------------------------------------ */

/* rr_pick_next() がキュー先頭タスクを返すことを確かめる */
static void test_rr_pick_next_returns_head(void)
{
	struct task_struct tsk_a, tsk_b;

	init_test_task(&tsk_a, RR_TIMESLICE);
	init_test_task(&tsk_b, RR_TIMESLICE);

	rr_enqueue(&tsk_a); /* 先に追加 → 先頭 */
	rr_enqueue(&tsk_b);

	KFS_ASSERT_TRUE(rr_pick_next() == &tsk_a);

	printk("rr_pick_next: returns head task OK\n");
}

/* キューが空のとき rr_pick_next() が NULL を返すことを確かめる */
static void test_rr_pick_next_empty_returns_null(void)
{
	KFS_ASSERT_TRUE(rr_pick_next() == 0);

	printk("rr_pick_next: empty queue returns NULL OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト: rr_task_tick                                                 */
/* ------------------------------------------------------------------ */

/* rr_task_tick() でタイムスライスが 1 減少することを確かめる */
static void test_rr_task_tick_decrements_slice(void)
{
	struct task_struct tsk;
	unsigned int before;

	init_test_task(&tsk, RR_TIMESLICE);
	rr_enqueue(&tsk);

	before = tsk.time_slice;
	rr_task_tick(&tsk);

	KFS_ASSERT_TRUE(tsk.time_slice == before - 1);

	printk("rr_task_tick: decrements slice %u -> %u OK\n", before, tsk.time_slice);
}

/* タイムスライスが 0 になったらキュー末尾に回されることを確かめる */
static void test_rr_task_tick_rotates_on_expiry(void)
{
	struct task_struct tsk_a, tsk_b;

	init_test_task(&tsk_a, 0); /* タイムスライス枯渇状態 */
	init_test_task(&tsk_b, RR_TIMESLICE);

	rr_enqueue(&tsk_a);
	rr_enqueue(&tsk_b);

	/* tsk_a の tick → 末尾へ回転、タイムスライスをリセット */
	rr_task_tick(&tsk_a);

	KFS_ASSERT_TRUE(tsk_a.time_slice == RR_TIMESLICE); /* リセットされた */
	KFS_ASSERT_TRUE(rr_pick_next() == &tsk_b);		   /* tsk_b が先頭に */

	printk("rr_task_tick: rotates task on expiry OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト: schedule()                                                   */
/* ------------------------------------------------------------------ */

/* rr_pick_next() が current と同じタスクのとき current が変わらないことを確かめる
（schedule()はタスクの切り替えをしないため，schedule()呼出後もタスクは変わらないはず） */
static void test_schedule_noop_when_same(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	rr_enqueue(&tsk);
	current = &tsk; /* current と先頭が同じ */

	schedule();

	KFS_ASSERT_TRUE(current == &tsk); /* 変わらない */

	printk("schedule: no-op when next == current OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト: wake_up_process()                                            */
/* ------------------------------------------------------------------ */

/* wake_up_process() でタスクが TASK_RUNNING になりキューに入ることを確かめる */
static void test_wake_up_process_enqueues(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	tsk.__state = TASK_INTERRUPTIBLE; /* スリープ状態に設定 */

	wake_up_process(&tsk);

	KFS_ASSERT_TRUE(tsk.__state == TASK_RUNNING); /* 起床した */
	KFS_ASSERT_TRUE(!list_empty(&tsk.run_list));  /* キューに登録された */
	KFS_ASSERT_TRUE(rr_pick_next() == &tsk);	  /* キュー先頭に現れる */

	printk("wake_up_process: task enqueued as TASK_RUNNING OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト登録                                                            */
/* ------------------------------------------------------------------ */

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_enqueue_adds_task, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_enqueue_no_duplicate, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_dequeue_removes_task, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_pick_next_returns_head, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_pick_next_empty_returns_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_task_tick_decrements_slice, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rr_task_tick_rotates_on_expiry, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_schedule_noop_when_same, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_wake_up_process_enqueues, setup_test, teardown_test),
};

int register_unit_tests_rr(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
