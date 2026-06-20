#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/string.h>

extern struct task_struct init_task;

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 必要なら後処理（現在は空） */
}

/**
 * test_init_task_initialization - init_taskの初期化検証
 *
 * init_taskが正しく初期化されているか確認
 */
static void test_init_task_initialization(void)
{
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

/* rr_enqueue/dequeue を直接呼んで schedule() の TASK_INTERRUPTIBLE パスを再現する */
static void init_test_task_for_sched(struct task_struct *tsk)
{
	tsk->policy = SCHED_PURE_RR;
	tsk->time_slice = 10;
	INIT_LIST_HEAD(&tsk->run_list);
}

static void setup_test_sched(void)
{
	reset_all_state_for_test();
	rr_init();
	INIT_LIST_HEAD(&init_task.run_list);
}

/** TASK_INTERRUPTIBLE なプロセスは schedule() 後にランキューに戻らないことを確かめる
 * 検証対象: kernel/sched/core.c schedule() の TASK_INTERRUPTIBLE チェック
 * 検証項目: __state == TASK_INTERRUPTIBLE のタスクは schedule() 呼び出し後に
 *           ランキューに再登録されない
 */
static void test_schedule_does_not_reenqueue_interruptible(void)
{
	struct task_struct tsk;
	struct task_struct *saved_current;

	init_test_task_for_sched(&tsk);
	tsk.__state = TASK_INTERRUPTIBLE;

	/* current を差し替え，schedule() に prev として認識させる */
	saved_current = current;
	current = &tsk;
	rr_enqueue(&tsk);
	KFS_ASSERT_TRUE(!list_empty(&tsk.run_list)); /* エンキュー済み */

	/* 実際に schedule() を呼ぶ.
	 * キューに tsk しかいないため rr_pick_next() は NULL を返し
	 * __switch_to() は呼ばれない. */
	schedule();

	current = saved_current;

	/* TASK_INTERRUPTIBLE なので再エンキューされていない */
	KFS_ASSERT_TRUE(list_empty(&tsk.run_list));
	printk("test_schedule_does_not_reenqueue_interruptible: OK\n");
}

/** TASK_RUNNING なプロセスは schedule() 後もランキューに残ることを確かめる
 * 検証対象: kernel/sched/core.c schedule() の TASK_RUNNING パス
 * 検証項目: __state == TASK_RUNNING のタスクは schedule() 呼び出し後も
 *           ランキューに残っている
 */
static void test_schedule_reenqueues_running(void)
{
	struct task_struct tsk;
	struct task_struct *saved_current;

	init_test_task_for_sched(&tsk);
	tsk.__state = TASK_RUNNING;

	/* current を差し替え，schedule() に prev として認識させる */
	saved_current = current;
	current = &tsk;
	rr_enqueue(&tsk);

	/* 実際に schedule() を呼ぶ.
	 * tsk は再エンキューされるが next == prev となるため
	 * __switch_to() は呼ばれない. */
	schedule();

	current = saved_current;

	/* TASK_RUNNING なので再エンキューされている */
	KFS_ASSERT_TRUE(!list_empty(&tsk.run_list));
	printk("test_schedule_reenqueues_running: OK\n");
}

/** RR のタイムスライスが残っている間は再スケジュール要求を出さない */
static void test_scheduler_tick_rr_no_resched_before_expiry(void)
{
	struct task_struct tsk;
	struct task_struct *saved_current;

	init_test_task_for_sched(&tsk);
	tsk.__state = TASK_RUNNING;
	tsk.flags = 0;
	tsk.time_slice = 2;
	saved_current = current;
	current = &tsk;
	scheduler_clear_need_resched();

	scheduler_tick();

	KFS_ASSERT_TRUE(tsk.time_slice == 1);
	KFS_ASSERT_TRUE(!scheduler_need_resched());
	current = saved_current;
	printk("scheduler_tick: RR no resched before expiry OK\n");
}

/** RR のタイムスライスが切れたら再スケジュール要求を出す */
static void test_scheduler_tick_rr_sets_resched_on_expiry(void)
{
	struct task_struct tsk;
	struct task_struct *saved_current;

	init_test_task_for_sched(&tsk);
	tsk.__state = TASK_RUNNING;
	tsk.flags = 0;
	tsk.time_slice = 1;
	saved_current = current;
	current = &tsk;
	scheduler_clear_need_resched();

	scheduler_tick();

	KFS_ASSERT_TRUE(tsk.time_slice == RR_TIMESLICE);
	KFS_ASSERT_TRUE(scheduler_need_resched());
	scheduler_clear_need_resched();
	current = saved_current;
	printk("scheduler_tick: RR resched on expiry OK\n");
}

/** CFS task は tick ごとに再スケジュール候補になる */
static void test_scheduler_tick_fair_sets_resched(void)
{
	struct task_struct tsk;
	struct task_struct *saved_current;

	init_test_task_for_sched(&tsk);
	tsk.__state = TASK_RUNNING;
	tsk.flags = 0;
	tsk.policy = SCHED_NORMAL;
	tsk.nice = 0;
	sched_init_entity(&tsk);
	saved_current = current;
	current = &tsk;
	scheduler_clear_need_resched();

	scheduler_tick();

	KFS_ASSERT_TRUE(scheduler_need_resched());
	scheduler_clear_need_resched();
	current = saved_current;
	printk("scheduler_tick: fair resched OK\n");
}

/** SCHED_EXT task は backend 未ロード時も fair fallback として再スケジュール候補になる */
static void test_scheduler_tick_sched_ext_sets_resched(void)
{
	struct task_struct tsk;
	struct task_struct *saved_current;

	init_test_task_for_sched(&tsk);
	tsk.__state = TASK_RUNNING;
	tsk.flags = 0;
	tsk.policy = SCHED_EXT;
	tsk.nice = 0;
	sched_init_entity(&tsk);
	saved_current = current;
	current = &tsk;
	scheduler_clear_need_resched();

	scheduler_tick();

	KFS_ASSERT_TRUE(scheduler_need_resched());
	scheduler_clear_need_resched();
	current = saved_current;
	printk("scheduler_tick: SCHED_EXT fair fallback resched OK\n");
}

/** ring-0 由来の割り込みではプリエンプトしない */
static void test_scheduler_return_work_ignores_kernel_regs(void)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.cs = 0x08;
	scheduler_clear_need_resched();
	current->policy = SCHED_NORMAL;
	scheduler_tick();
	KFS_ASSERT_TRUE(scheduler_need_resched());

	scheduler_return_to_user_work(&regs);

	KFS_ASSERT_TRUE(scheduler_need_resched());
	scheduler_clear_need_resched();
	printk("scheduler_return_to_user_work: kernel regs ignored OK\n");
}

/** ring-3 復帰前に保留シグナルを処理する */
static void test_scheduler_return_work_handles_pending_signal(void)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.cs = 0x1b;
	regs.esp = 0x800000;
	regs.eip = 0x400000;
	current->flags = 0;
	current->pending.signal = 0;
	current->sig_actions[SIGUSR1].sa_handler = SIG_IGN;
	scheduler_clear_need_resched();
	send_signal(SIGUSR1, current);
	KFS_ASSERT_TRUE(signal_pending());

	scheduler_return_to_user_work(&regs);

	KFS_ASSERT_TRUE(!signal_pending());
	current->sig_actions[SIGUSR1].sa_handler = SIG_DFL;
	printk("scheduler_return_to_user_work: pending signal handled OK\n");
}

/** 実行中 task への wake-up で runqueue へ二重登録しないことを確かめる */
static void test_wake_up_current_does_not_enqueue(void)
{
	KFS_ASSERT_TRUE(current->__state == TASK_RUNNING);
	KFS_ASSERT_TRUE(!sched_task_queued(current));

	wake_up_process(current);

	KFS_ASSERT_TRUE(!sched_task_queued(current));
	printk("wake_up_process: current task is not enqueued twice OK\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_init_task_initialization, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_list_operations, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_task_state_constants, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_wake_up_current_does_not_enqueue, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_schedule_does_not_reenqueue_interruptible, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_schedule_reenqueues_running, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scheduler_tick_rr_no_resched_before_expiry, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scheduler_tick_rr_sets_resched_on_expiry, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scheduler_tick_fair_sets_resched, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scheduler_tick_sched_ext_sets_resched, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scheduler_return_work_ignores_kernel_regs, setup_test_sched, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scheduler_return_work_handles_pending_signal, setup_test_sched, teardown_test),
};

int register_unit_tests_sched_core(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
