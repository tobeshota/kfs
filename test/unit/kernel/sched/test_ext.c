#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/exit.h>
#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/sched_ext.h>
#include <kfs/string.h>

extern struct list_head task_list;

static struct task_struct *fake_next_task;
static struct task_struct *fake_pick_override;
static int fake_pick_returns_null;
static int fake_init_called;
static int fake_exit_called;
static int fake_tick_called;

/* テスト用 SCHED_EXT task を最小限に初期化する */
static void init_ext_test_task(struct task_struct *tsk, pid_t pid)
{
	memset(tsk, 0, sizeof(*tsk));
	tsk->__state = TASK_RUNNING;
	tsk->pid = pid;
	tsk->policy = SCHED_EXT;
	tsk->nice = 0;
	tsk->time_slice = 10;
	INIT_LIST_HEAD(&tsk->run_list);
	sched_init_entity(tsk);
}

static void reset_fake_backend(void)
{
	fake_next_task = NULL;
	fake_pick_override = NULL;
	fake_pick_returns_null = 0;
	fake_init_called = 0;
	fake_exit_called = 0;
	fake_tick_called = 0;
}

static int fake_backend_init(void)
{
	fake_init_called++;
	return 0;
}

static void fake_backend_exit(void)
{
	fake_exit_called++;
	fake_next_task = NULL;
	fake_pick_override = NULL;
	fake_pick_returns_null = 0;
}

static void fake_backend_enqueue(struct task_struct *task)
{
	fake_next_task = task;
}

static void fake_backend_dequeue(struct task_struct *task)
{
	if (fake_next_task == task)
	{
		fake_next_task = NULL;
	}
}

static int fake_backend_task_queued(struct task_struct *task)
{
	return fake_next_task == task;
}

static struct task_struct *fake_backend_pick_next_task(void)
{
	if (fake_pick_returns_null)
	{
		return NULL;
	}
	if (fake_pick_override)
	{
		return fake_pick_override;
	}
	return fake_next_task;
}

static void fake_backend_task_tick(struct task_struct *task)
{
	(void)task;
	fake_tick_called++;
}

static const struct sched_ext_ops fake_backend_ops = {
	.name = "fake_ext",
	.flags = 0,
	.init = fake_backend_init,
	.exit = fake_backend_exit,
	.enqueue_task = fake_backend_enqueue,
	.dequeue_task = fake_backend_dequeue,
	.task_queued = fake_backend_task_queued,
	.pick_next_task = fake_backend_pick_next_task,
	.task_tick = fake_backend_task_tick,
};

static const struct sched_ext_ops fake_partial_backend_ops = {
	.name = "fake_ext_partial",
	.flags = SCX_OPS_SWITCH_PARTIAL,
	.init = fake_backend_init,
	.exit = fake_backend_exit,
	.enqueue_task = fake_backend_enqueue,
	.dequeue_task = fake_backend_dequeue,
	.task_queued = fake_backend_task_queued,
	.pick_next_task = fake_backend_pick_next_task,
	.task_tick = fake_backend_task_tick,
};

static void setup_test(void)
{
	reset_all_state_for_test();
	sched_ext_unregister();
	reset_fake_backend();
}

static void teardown_test(void)
{
	sched_ext_unregister();
	reset_fake_backend();
}

static void test_sched_ext_starts_disabled(void)
{
	KFS_ASSERT_TRUE(!sched_ext_enabled());
	KFS_ASSERT_TRUE(strcmp(sched_ext_name(), "none") == 0);

	printk("sched_ext: starts disabled OK\n");
}

static void test_sched_ext_disabled_falls_back_to_fair(void)
{
	struct task_struct tsk;

	init_ext_test_task(&tsk, 10);

	sched_enqueue_task(&tsk);

	KFS_ASSERT_TRUE(sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(tsk.se.on_rq);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &tsk);

	sched_dequeue_task(&tsk);
	KFS_ASSERT_TRUE(!sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(!tsk.se.on_rq);

	printk("sched_ext: disabled backend falls back to fair OK\n");
}

static void test_sched_ext_register_enables_backend(void)
{
	KFS_ASSERT_TRUE(sched_ext_register(&fake_backend_ops) == 0);

	KFS_ASSERT_TRUE(sched_ext_enabled());
	KFS_ASSERT_TRUE(strcmp(sched_ext_name(), "fake_ext") == 0);
	KFS_ASSERT_TRUE(fake_init_called == 1);

	printk("sched_ext: register enables backend OK\n");
}

static void test_sched_ext_enabled_uses_backend(void)
{
	struct task_struct tsk;

	init_ext_test_task(&tsk, 11);
	INIT_LIST_HEAD(&tsk.tasks);
	list_add_tail(&tsk.tasks, &task_list);
	KFS_ASSERT_TRUE(sched_ext_register(&fake_backend_ops) == 0);

	sched_enqueue_task(&tsk);

	KFS_ASSERT_TRUE(sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(!tsk.se.on_rq);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &tsk);

	sched_task_tick(&tsk);
	KFS_ASSERT_TRUE(fake_tick_called == 1);

	sched_dequeue_task(&tsk);
	KFS_ASSERT_TRUE(!sched_task_queued(&tsk));
	list_del(&tsk.tasks);

	printk("sched_ext: enabled backend handles SCHED_EXT task OK\n");
}

static void test_sched_ext_unregister_returns_to_fair_fallback(void)
{
	struct task_struct tsk;

	init_ext_test_task(&tsk, 12);
	KFS_ASSERT_TRUE(sched_ext_register(&fake_backend_ops) == 0);

	sched_ext_unregister();
	KFS_ASSERT_TRUE(!sched_ext_enabled());
	KFS_ASSERT_TRUE(fake_exit_called == 1);

	sched_enqueue_task(&tsk);
	KFS_ASSERT_TRUE(sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(tsk.se.on_rq);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &tsk);

	printk("sched_ext: unregister returns to fair fallback OK\n");
}

static void test_sched_ext_pure_rr_preserves_rr_order(void)
{
	struct task_struct first;
	struct task_struct second;

	init_ext_test_task(&first, 20);
	init_ext_test_task(&second, 21);
	INIT_LIST_HEAD(&first.tasks);
	INIT_LIST_HEAD(&second.tasks);
	list_add_tail(&first.tasks, &task_list);
	list_add_tail(&second.tasks, &task_list);
	first.time_slice = 0;

	KFS_ASSERT_TRUE(sched_ext_register(&sched_ext_pure_rr_ops) == 0);
	KFS_ASSERT_TRUE(strcmp(sched_ext_name(), "pure_rr") == 0);

	sched_enqueue_task(&first);
	sched_enqueue_task(&second);

	KFS_ASSERT_TRUE(first.policy == SCHED_EXT);
	KFS_ASSERT_TRUE(second.policy == SCHED_EXT);
	KFS_ASSERT_TRUE(!first.se.on_rq);
	KFS_ASSERT_TRUE(!second.se.on_rq);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &first);

	sched_task_tick(&first);
	KFS_ASSERT_TRUE(first.time_slice == RR_TIMESLICE);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &second);

	sched_dequeue_task(&first);
	sched_dequeue_task(&second);
	list_del(&first.tasks);
	list_del(&second.tasks);

	printk("sched_ext: pure_rr backend preserves RR order OK\n");
}

static void test_sched_ext_pure_rr_isolated_from_legacy_queue(void)
{
	struct task_struct ext_task;
	struct task_struct legacy_task;

	init_ext_test_task(&ext_task, 22);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&ext_task.tasks, &task_list);
	memset(&legacy_task, 0, sizeof(legacy_task));
	legacy_task.__state = TASK_RUNNING;
	legacy_task.pid = 23;
	legacy_task.policy = SCHED_NORMAL;
	legacy_task.time_slice = RR_TIMESLICE;
	INIT_LIST_HEAD(&legacy_task.run_list);

	KFS_ASSERT_TRUE(sched_ext_register(&sched_ext_pure_rr_ops) == 0);
	rr_enqueue(&legacy_task);
	sched_enqueue_task(&ext_task);

	KFS_ASSERT_TRUE(sched_ext_class.pick_next_task() == &ext_task);
	KFS_ASSERT_TRUE(rr_pick_next() == &legacy_task);

	sched_dequeue_task(&ext_task);
	rr_dequeue(&legacy_task);
	list_del(&ext_task.tasks);

	printk("sched_ext: pure_rr queue is isolated from legacy RR OK\n");
}

static void test_sched_ext_syscall_load_status_unload(void)
{
	struct sched_ext_status status;
	struct task_struct owner;
	struct task_struct *saved_current = current;

	memset(&owner, 0, sizeof(owner));
	owner.pid = 42;
	current = &owner;

	KFS_ASSERT_TRUE(sys_sched_ext_load("pure_rr") == 0);
	KFS_ASSERT_TRUE(sys_sched_ext_status(&status) == 0);
	KFS_ASSERT_TRUE(status.enabled);
	KFS_ASSERT_TRUE(status.owner_pid == 42);
	KFS_ASSERT_TRUE(strcmp(status.name, "pure_rr") == 0);
	KFS_ASSERT_TRUE(strcmp(status.mode, "full") == 0);
	KFS_ASSERT_TRUE(sys_sched_ext_unload() == 0);
	KFS_ASSERT_TRUE(!sched_ext_enabled());

	current = saved_current;
	printk("sched_ext: load status unload syscall API OK\n");
}

static void test_sched_ext_unload_rejects_non_owner(void)
{
	struct task_struct owner;
	struct task_struct other;
	struct task_struct *saved_current = current;

	memset(&owner, 0, sizeof(owner));
	memset(&other, 0, sizeof(other));
	owner.pid = 43;
	other.pid = 44;
	current = &owner;
	KFS_ASSERT_TRUE(sys_sched_ext_load("pure_rr") == 0);

	current = &other;
	KFS_ASSERT_TRUE(sys_sched_ext_unload() == -EPERM);
	KFS_ASSERT_TRUE(sched_ext_enabled());

	current = &owner;
	KFS_ASSERT_TRUE(sys_sched_ext_unload() == 0);
	current = saved_current;
	printk("sched_ext: non-owner unload rejected OK\n");
}

static void test_sched_ext_owner_exit_unloads_backend(void)
{
	struct task_struct owner;
	struct task_struct *saved_current = current;

	memset(&owner, 0, sizeof(owner));
	owner.pid = 45;
	current = &owner;
	KFS_ASSERT_TRUE(sys_sched_ext_load("pure_rr") == 0);

	invoke_exit_hooks(&owner);
	KFS_ASSERT_TRUE(!sched_ext_enabled());

	current = saved_current;
	printk("sched_ext: owner exit unloads backend OK\n");
}

static void test_sched_ext_unload_migrates_tasks_to_fair(void)
{
	struct task_struct owner;
	struct task_struct ext_task;
	struct task_struct *saved_current = current;

	memset(&owner, 0, sizeof(owner));
	owner.pid = 46;
	init_ext_test_task(&ext_task, 47);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&ext_task.tasks, &task_list);

	current = &owner;
	KFS_ASSERT_TRUE(sys_sched_ext_load("pure_rr") == 0);
	sched_enqueue_task(&ext_task);
	KFS_ASSERT_TRUE(!ext_task.se.on_rq);

	KFS_ASSERT_TRUE(sys_sched_ext_unload() == 0);
	KFS_ASSERT_TRUE(ext_task.se.on_rq);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &ext_task);

	sched_dequeue_task(&ext_task);
	list_del(&ext_task.tasks);
	current = saved_current;
	printk("sched_ext: unload migrates tasks to fair OK\n");
}

static void test_sched_ext_load_migrates_fair_fallback_tasks(void)
{
	struct task_struct owner;
	struct task_struct ext_task;
	struct task_struct *saved_current = current;

	memset(&owner, 0, sizeof(owner));
	owner.pid = 48;
	init_ext_test_task(&ext_task, 49);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&ext_task.tasks, &task_list);

	sched_enqueue_task(&ext_task);
	KFS_ASSERT_TRUE(ext_task.se.on_rq);

	current = &owner;
	KFS_ASSERT_TRUE(sys_sched_ext_load("pure_rr") == 0);
	KFS_ASSERT_TRUE(!ext_task.se.on_rq);
	KFS_ASSERT_TRUE(sched_ext_class.task_queued(&ext_task));
	KFS_ASSERT_TRUE(sched_ext_class.pick_next_task() == &ext_task);

	sched_dequeue_task(&ext_task);
	list_del(&ext_task.tasks);
	current = saved_current;
	printk("sched_ext: load migrates fair fallback tasks to backend OK\n");
}

static void test_sched_ext_partial_mode_keeps_fair_precedence(void)
{
	struct task_struct fair_task;
	struct task_struct ext_task;

	memset(&fair_task, 0, sizeof(fair_task));
	fair_task.__state = TASK_RUNNING;
	fair_task.pid = 50;
	fair_task.policy = SCHED_NORMAL;
	fair_task.nice = 0;
	INIT_LIST_HEAD(&fair_task.run_list);
	sched_init_entity(&fair_task);
	init_ext_test_task(&ext_task, 51);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&ext_task.tasks, &task_list);

	KFS_ASSERT_TRUE(sched_ext_register(&fake_partial_backend_ops) == 0);
	KFS_ASSERT_TRUE(sched_ext_switch_mode() == SCHED_EXT_MODE_PARTIAL);
	sched_enqueue_task(&fair_task);
	sched_enqueue_task(&ext_task);

	KFS_ASSERT_TRUE(sched_pick_next_task() == &fair_task);

	sched_dequeue_task(&fair_task);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &ext_task);
	sched_dequeue_task(&ext_task);
	list_del(&ext_task.tasks);
	printk("sched_ext: partial mode keeps fair precedence OK\n");
}

static void test_sched_ext_full_mode_routes_normal_and_ext_to_backend(void)
{
	struct task_struct normal_task;
	struct task_struct ext_task;

	memset(&normal_task, 0, sizeof(normal_task));
	normal_task.__state = TASK_RUNNING;
	normal_task.pid = 55;
	normal_task.policy = SCHED_NORMAL;
	normal_task.nice = 0;
	INIT_LIST_HEAD(&normal_task.run_list);
	sched_init_entity(&normal_task);
	init_ext_test_task(&ext_task, 56);
	INIT_LIST_HEAD(&normal_task.tasks);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&normal_task.tasks, &task_list);
	list_add_tail(&ext_task.tasks, &task_list);

	KFS_ASSERT_TRUE(sched_ext_register(&sched_ext_pure_rr_ops) == 0);
	KFS_ASSERT_TRUE(sched_ext_switch_mode() == SCHED_EXT_MODE_FULL);
	sched_enqueue_task(&normal_task);
	sched_enqueue_task(&ext_task);

	KFS_ASSERT_TRUE(!normal_task.se.on_rq);
	KFS_ASSERT_TRUE(!ext_task.se.on_rq);
	KFS_ASSERT_TRUE(sched_ext_class.task_queued(&normal_task));
	KFS_ASSERT_TRUE(sched_ext_class.task_queued(&ext_task));

	sched_dequeue_task(&normal_task);
	sched_dequeue_task(&ext_task);
	list_del(&normal_task.tasks);
	list_del(&ext_task.tasks);
	printk("sched_ext: full mode routes normal and ext to backend OK\n");
}

/** backendがqueued taskをdispatchしない場合にfairへfallbackすることを確かめる */
static void test_sched_ext_dispatch_empty_falls_back_to_fair(void)
{
	struct task_struct ext_task;
	struct sched_ext_status status;

	init_ext_test_task(&ext_task, 52);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&ext_task.tasks, &task_list);
	KFS_ASSERT_TRUE(sched_ext_register(&fake_backend_ops) == 0);
	sched_enqueue_task(&ext_task);
	fake_pick_returns_null = 1;

	KFS_ASSERT_TRUE(sched_ext_class.pick_next_task() == &ext_task);
	KFS_ASSERT_TRUE(!sched_ext_enabled());
	KFS_ASSERT_TRUE(ext_task.se.on_rq);
	KFS_ASSERT_TRUE(fake_exit_called == 1);
	KFS_ASSERT_TRUE(sys_sched_ext_status(&status) == 0);
	KFS_ASSERT_TRUE(strcmp(status.fallback_reason, "dispatch_empty") == 0);

	sched_dequeue_task(&ext_task);
	list_del(&ext_task.tasks);
	printk("sched_ext: empty dispatch falls back to fair OK\n");
}

/** backendが管理外taskを返した場合にfairへfallbackすることを確かめる */
static void test_sched_ext_invalid_task_falls_back_to_fair(void)
{
	struct task_struct ext_task;
	struct task_struct invalid_task;
	struct sched_ext_status status;

	init_ext_test_task(&ext_task, 53);
	init_ext_test_task(&invalid_task, 54);
	INIT_LIST_HEAD(&ext_task.tasks);
	list_add_tail(&ext_task.tasks, &task_list);
	KFS_ASSERT_TRUE(sched_ext_register(&fake_backend_ops) == 0);
	sched_enqueue_task(&ext_task);
	fake_pick_override = &invalid_task;

	KFS_ASSERT_TRUE(sched_ext_class.pick_next_task() == &ext_task);
	KFS_ASSERT_TRUE(!sched_ext_enabled());
	KFS_ASSERT_TRUE(ext_task.se.on_rq);
	KFS_ASSERT_TRUE(fake_exit_called == 1);
	KFS_ASSERT_TRUE(sys_sched_ext_status(&status) == 0);
	KFS_ASSERT_TRUE(strcmp(status.fallback_reason, "invalid_task") == 0);

	sched_dequeue_task(&ext_task);
	list_del(&ext_task.tasks);
	printk("sched_ext: invalid task falls back to fair OK\n");
}

static struct kfs_test_case ext_tests[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_starts_disabled, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_disabled_falls_back_to_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_register_enables_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_enabled_uses_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_unregister_returns_to_fair_fallback, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_pure_rr_preserves_rr_order, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_pure_rr_isolated_from_legacy_queue, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_syscall_load_status_unload, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_unload_rejects_non_owner, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_owner_exit_unloads_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_unload_migrates_tasks_to_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_load_migrates_fair_fallback_tasks, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_partial_mode_keeps_fair_precedence, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_full_mode_routes_normal_and_ext_to_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_dispatch_empty_falls_back_to_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_invalid_task_falls_back_to_fair, setup_test, teardown_test),
};

int register_unit_tests_ext(struct kfs_test_case **out)
{
	*out = ext_tests;
	return sizeof(ext_tests) / sizeof(ext_tests[0]);
}
