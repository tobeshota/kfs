#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/list.h>
#include <kfs/sched.h>
#include <kfs/sched_ext.h>
#include <kfs/string.h>

static struct task_struct *fake_next_task;
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
	return fake_next_task;
}

static void fake_backend_task_tick(struct task_struct *task)
{
	(void)task;
	fake_tick_called++;
}

static const struct sched_ext_ops fake_backend_ops = {
	.name = "fake_ext",
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
	KFS_ASSERT_TRUE(sched_ext_register(&fake_backend_ops) == 0);

	sched_enqueue_task(&tsk);

	KFS_ASSERT_TRUE(sched_task_queued(&tsk));
	KFS_ASSERT_TRUE(!tsk.se.on_rq);
	KFS_ASSERT_TRUE(sched_pick_next_task() == &tsk);

	sched_task_tick(&tsk);
	KFS_ASSERT_TRUE(fake_tick_called == 1);

	sched_dequeue_task(&tsk);
	KFS_ASSERT_TRUE(!sched_task_queued(&tsk));

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

static struct kfs_test_case ext_tests[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_starts_disabled, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_disabled_falls_back_to_fair, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_register_enables_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_enabled_uses_backend, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_unregister_returns_to_fair_fallback, setup_test, teardown_test),
};

int register_unit_tests_ext(struct kfs_test_case **out)
{
	*out = ext_tests;
	return sizeof(ext_tests) / sizeof(ext_tests[0]);
}
