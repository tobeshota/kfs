#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/capability.h>
#include <kfs/errno.h>
#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/sys.h>
#include <kfs/unistd.h>

extern struct task_struct init_task;
extern struct list_head task_list;

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

/* scheduler core の enqueue API が SCHED_PURE_RR を pure RR runqueue に委譲することを確かめる */
static void test_sched_enqueue_task_pure_rr_uses_rr_queue(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	tsk.policy = SCHED_PURE_RR;

	sched_enqueue_task(&tsk);

	KFS_ASSERT_TRUE(sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(rr_pick_next() == &tsk);

	printk("sched_enqueue_task: SCHED_PURE_RR uses RR runqueue OK\n");
}

/* scheduler core の dequeue API が SCHED_PURE_RR task を pure RR runqueue から外すことを確かめる */
static void test_sched_dequeue_task_pure_rr_removes_from_rr_queue(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	tsk.policy = SCHED_PURE_RR;
	sched_enqueue_task(&tsk);

	sched_dequeue_task(&tsk);

	KFS_ASSERT_TRUE(!sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(rr_pick_next() == 0);

	printk("sched_dequeue_task: SCHED_PURE_RR removed from RR runqueue OK\n");
}

/* scheduler core の tick API が SCHED_PURE_RR task の RR time slice を進めることを確かめる */
static void test_sched_task_tick_pure_rr_decrements_slice(void)
{
	struct task_struct tsk;

	init_test_task(&tsk, RR_TIMESLICE);
	tsk.policy = SCHED_PURE_RR;
	sched_enqueue_task(&tsk);

	sched_task_tick(&tsk);

	KFS_ASSERT_TRUE(tsk.time_slice == RR_TIMESLICE - 1);
	KFS_ASSERT_TRUE(sched_task_queued(&tsk));

	printk("sched_task_tick: SCHED_PURE_RR decrements RR slice OK\n");
}

/* ------------------------------------------------------------------ */
/* テスト: sys_sched_setscheduler() / sys_sched_getscheduler()          */
/* ------------------------------------------------------------------ */

/* SCHED_PURE_RR は RT ではないため CAP_SYS_NICE なしでも設定できることを確かめる */
static void test_sched_setscheduler_ok(void)
{
	current->cap_effective = CAP_EMPTY_SET; /* 権限なし */
	current->policy = SCHED_NORMAL;

	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_PURE_RR, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_PURE_RR);

	printk("sys_sched_setscheduler: ok OK\n");
}

/* SCHED_NORMAL は通常プロセス用 policy として受理されることを確かめる */
static void test_sched_setscheduler_accepts_sched_normal(void)
{
	current->cap_effective = CAP_EMPTY_SET;
	current->policy = SCHED_PURE_RR;
	current->rt_priority = 7;

	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_NORMAL, 0) == 0); /* SCHED_NORMAL に設定できることを確かめる */
	KFS_ASSERT_TRUE(current->policy == SCHED_NORMAL); /* ポリシーが SCHED_NORMAL に設定されたことを確認する */
	KFS_ASSERT_TRUE(current->rt_priority == 0); /* RT 優先度がクリアされたことを確認する */

	printk("sys_sched_setscheduler: accepts SCHED_NORMAL OK\n");
}

/* SCHED_EXT は backend 未ロードでも policy として受理される */
static void test_sched_setscheduler_accepts_sched_ext(void)
{
	current->cap_effective = CAP_EMPTY_SET;
	current->policy = SCHED_NORMAL;
	current->rt_priority = 7;

	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_EXT, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_EXT);
	KFS_ASSERT_TRUE(current->rt_priority == 0);

	printk("sys_sched_setscheduler: accepts SCHED_EXT OK\n");
}

/* 不正なポリシー番号を渡すと -EINVAL が返ることを確かめる */
static void test_sched_setscheduler_einval(void)
{
	// ポリシー番号は SCHED_* 定数以外は無効（Linux 6.18 準拠）
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, 999, 0) == -EINVAL);

	printk("sys_sched_setscheduler: -EINVAL OK\n");
}

/* kfs 未実装の Linux RT policy は明示的に -EINVAL で拒否される */
static void test_sched_setscheduler_rejects_linux_rt_policy(void)
{
	current->cap_effective = CAP_FULL_SET;

	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_FIFO, 0) == -EINVAL);
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_RR, 0) == -EINVAL);
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_DEADLINE, 0) == -EINVAL);

	printk("sys_sched_setscheduler: rejects Linux RT policies OK\n");
}

/* queued task を pure RR から fair runqueue へ移動できることを確かめる */
static void test_sched_setscheduler_migrates_rr_to_fair(void)
{
	current->policy = SCHED_PURE_RR;
	current->time_slice = RR_TIMESLICE;
	sched_enqueue_task(current);

	KFS_ASSERT_TRUE(sched_task_queued(current));
	KFS_ASSERT_TRUE(!list_empty(&current->run_list));
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_NORMAL, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_NORMAL);
	KFS_ASSERT_TRUE(list_empty(&current->run_list));
	KFS_ASSERT_TRUE(sched_task_queued(current));

	sched_dequeue_task(current);
	printk("sys_sched_setscheduler: migrates RR to fair OK\n");
}

/* SCHED_EXT は backend 未ロード時に fair runqueue へ fallback する */
static void test_sched_setscheduler_ext_fallback_uses_fair(void)
{
	current->policy = SCHED_PURE_RR;
	current->time_slice = RR_TIMESLICE;
	sched_enqueue_task(current);

	KFS_ASSERT_TRUE(sched_task_queued(current));
	KFS_ASSERT_TRUE(!list_empty(&current->run_list));
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_EXT, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_EXT);
	KFS_ASSERT_TRUE(list_empty(&current->run_list));
	KFS_ASSERT_TRUE(current->se.on_rq);
	KFS_ASSERT_TRUE(sched_task_queued(current));

	sched_dequeue_task(current);
	printk("sys_sched_setscheduler: SCHED_EXT fair fallback OK\n");
}

/* queued task を fair から pure RR runqueue へ移動できることを確かめる */
static void test_sched_setscheduler_migrates_fair_to_rr(void)
{
	current->policy = SCHED_NORMAL;
	current->se.vruntime = 0;
	sched_init_entity(current);
	sched_enqueue_task(current);

	KFS_ASSERT_TRUE(sched_task_queued(current));
	KFS_ASSERT_TRUE(current->se.on_rq);
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_PURE_RR, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_PURE_RR);
	KFS_ASSERT_TRUE(!current->se.on_rq);
	KFS_ASSERT_TRUE(sched_task_queued(current));
	KFS_ASSERT_TRUE(!list_empty(&current->run_list));

	sched_dequeue_task(current);
	printk("sys_sched_setscheduler: migrates fair to RR OK\n");
}

/* backend有効時にqueued taskをfairからsched_ext runqueueへ移動できることを確かめる */
static void test_sched_setscheduler_migrates_fair_to_ext_backend(void)
{
	current->policy = SCHED_NORMAL;
	current->se.vruntime = 0;
	sched_init_entity(current);
	sched_enqueue_task(current);
	KFS_ASSERT_TRUE(sched_ext_register(&sched_ext_pure_rr_ops) == 0);

	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_EXT, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_EXT);
	KFS_ASSERT_TRUE(!current->se.on_rq);
	KFS_ASSERT_TRUE(sched_ext_class.task_queued(current));

	sched_dequeue_task(current);
	printk("sys_sched_setscheduler: migrates fair to ext backend OK\n");
}

/* backend有効時にqueued taskをsched_extからfair runqueueへ戻せることを確かめる */
static void test_sched_setscheduler_migrates_ext_backend_to_fair(void)
{
	current->policy = SCHED_EXT;
	current->time_slice = RR_TIMESLICE;
	KFS_ASSERT_TRUE(sched_ext_register(&sched_ext_pure_rr_ops) == 0);
	sched_enqueue_task(current);

	KFS_ASSERT_TRUE(sched_ext_class.task_queued(current));
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_NORMAL, 0) == 0);
	KFS_ASSERT_TRUE(current->policy == SCHED_NORMAL);
	KFS_ASSERT_TRUE(list_empty(&current->run_list));
	KFS_ASSERT_TRUE(current->se.on_rq);
	KFS_ASSERT_TRUE(sched_task_queued(current));

	sched_dequeue_task(current);
	printk("sys_sched_setscheduler: migrates ext backend to fair OK\n");
}

/* sleep中のtaskはpolicy変更後もrunqueue外に留まることを確かめる */
static void test_sched_setscheduler_sleeping_task_stays_dequeued(void)
{
	struct task_struct target = *current;

	target.pid = 52;
	target.policy = SCHED_NORMAL;
	target.__state = TASK_INTERRUPTIBLE;
	INIT_LIST_HEAD(&target.tasks);
	INIT_LIST_HEAD(&target.run_list);
	sched_init_entity(&target);
	list_add_tail(&target.tasks, &task_list);

	KFS_ASSERT_TRUE(sys_sched_setscheduler(target.pid, SCHED_EXT, 0) == 0);
	KFS_ASSERT_TRUE(target.policy == SCHED_EXT);
	KFS_ASSERT_TRUE(!sched_task_queued(&target));

	list_del(&target.tasks);
	printk("sys_sched_setscheduler: sleeping task stays dequeued OK\n");
}

/* sys_sched_getscheduler() が pid 0 のとき current のポリシーを返すことを確かめる */
static void test_sched_getscheduler(void)
{
	current->policy = SCHED_PURE_RR;

	KFS_ASSERT_TRUE(sys_sched_getscheduler(0) == SCHED_PURE_RR);

	printk("sys_sched_getscheduler: returns policy OK\n");
}

/* sys_sched_getscheduler() は SCHED_EXT policy もそのまま返す */
static void test_sched_getscheduler_returns_sched_ext(void)
{
	current->policy = SCHED_EXT;

	KFS_ASSERT_TRUE(sys_sched_getscheduler(0) == SCHED_EXT);

	printk("sys_sched_getscheduler: returns SCHED_EXT OK\n");
}

/* SCHED_EXT と SCHED_NORMAL の往復後も、他の fair task の順序と参照が保たれることを確かめる */
static void test_sched_setscheduler_ext_round_trip_preserves_fair_runqueue(void)
{
	struct task_struct first = {0};
	struct task_struct second = {0};
	struct task_struct target = {0};

	first.__state = TASK_RUNNING;
	first.pid = 60;
	first.policy = SCHED_NORMAL;
	first.nice = 0;
	first.se.vruntime = 10;
	INIT_LIST_HEAD(&first.run_list);
	sched_init_entity(&first);

	second.__state = TASK_RUNNING;
	second.pid = 61;
	second.policy = SCHED_NORMAL;
	second.nice = 0;
	second.se.vruntime = 20;
	INIT_LIST_HEAD(&second.run_list);
	sched_init_entity(&second);

	target.__state = TASK_RUNNING;
	target.pid = 62;
	target.policy = SCHED_NORMAL;
	target.nice = 0;
	target.se.vruntime = 30;
	INIT_LIST_HEAD(&target.tasks);
	INIT_LIST_HEAD(&target.run_list);
	sched_init_entity(&target);
	list_add_tail(&target.tasks, &task_list);

	sched_enqueue_task(&first);
	sched_enqueue_task(&second);
	sched_enqueue_task(&target);
	KFS_ASSERT_TRUE(sched_ext_register(&sched_ext_pure_rr_ops) == 0);

	KFS_ASSERT_TRUE(sys_sched_setscheduler(target.pid, SCHED_EXT, 0) == 0);
	KFS_ASSERT_TRUE(sys_sched_setscheduler(target.pid, SCHED_NORMAL, 0) == 0);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &first);
	sched_dequeue_task(&first);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &second);
	sched_dequeue_task(&second);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &target);
	sched_dequeue_task(&target);

	list_del(&target.tasks);
	sched_ext_unregister();
	printk("sys_sched_setscheduler: EXT round trip preserves fair runqueue OK\n");
}

/* 非 RT ポリシーに priority != 0 を渡すと -EINVAL になることを確かめる（Linux 6.18 準拠） */
static void test_sched_setscheduler_non_rt_clears_priority(void)
{
	KFS_ASSERT_TRUE(sys_sched_setscheduler(0, SCHED_PURE_RR, 42) == -EINVAL);

	printk("sys_sched_setscheduler: non-RT with priority!=0 returns -EINVAL OK\n");
}

/* sched_setscheduler() wrapper は NULL param を -EINVAL として拒否する */
static void test_sched_setscheduler_wrapper_rejects_null_param(void)
{
	KFS_ASSERT_TRUE(sched_setscheduler(0, SCHED_NORMAL, 0) == -EINVAL);

	printk("sched_setscheduler wrapper: NULL param rejected OK\n");
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
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_enqueue_task_pure_rr_uses_rr_queue, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_dequeue_task_pure_rr_removes_from_rr_queue, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_task_tick_pure_rr_decrements_slice, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_ok, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_accepts_sched_normal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_accepts_sched_ext, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_einval, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_rejects_linux_rt_policy, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_migrates_rr_to_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_ext_fallback_uses_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_migrates_fair_to_rr, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_migrates_fair_to_ext_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_migrates_ext_backend_to_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_sleeping_task_stays_dequeued, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_ext_round_trip_preserves_fair_runqueue, setup_test,
								 teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_getscheduler, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_getscheduler_returns_sched_ext, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_non_rt_clears_priority, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_setscheduler_wrapper_rejects_null_param, setup_test, teardown_test),
};

int register_unit_tests_rr(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
