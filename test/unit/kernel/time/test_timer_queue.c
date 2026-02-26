#include "../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/timer.h>

/* timer_test_shim.c で定義 */
extern void timer_queue_reset(void);

static volatile int g_callback_count;

static void test_callback(struct timer_list *t)
{
	(void)t;
	g_callback_count++;
}

static void setup_test(void)
{
	reset_all_state_for_test();
	timer_queue_reset();
	g_callback_count = 0;
}

static void teardown_test(void)
{
}

/** タイマーが expires tick ちょうどで発火することを確かめる
 * 検証対象: add_timer(), run_local_timers()
 * 検証項目:
 *   - expires-1 tick では callback が呼ばれない
 *   - expires tick 目で callback が 1 回呼ばれる
 */
static void test_timer_fires_at_expiry(void)
{
	struct timer_list t;

	timer_setup(&t, test_callback);
	t.expires = jiffies + 10;
	add_timer(&t);

	/* 9 tick — まだ満了しない */
	for (int i = 0; i < 9; i++)
	{
		jiffies++;
		run_local_timers();
	}
	KFS_ASSERT_EQ(0, g_callback_count);

	/* 10 tick 目 — 満了して callback が呼ばれる */
	jiffies++;
	run_local_timers();
	KFS_ASSERT_EQ(1, g_callback_count);

	printk("test_timer_fires_at_expiry: OK (jiffies=%u)\n", jiffies);
}

/** del_timer() がタイマーをキャンセルすることを確かめる
 * 検証対象: del_timer()
 * 検証項目: add_timer 後に del_timer すると callback が一切呼ばれない
 */
static void test_del_timer_cancels(void)
{
	struct timer_list t;

	timer_setup(&t, test_callback);
	t.expires = jiffies + 5;
	add_timer(&t);
	del_timer(&t);

	for (int i = 0; i < 10; i++)
	{
		jiffies++;
		run_local_timers();
	}
	KFS_ASSERT_EQ(0, g_callback_count);

	printk("test_del_timer_cancels: OK\n");
}

static int g_order[3];
static int g_order_idx;

/* cb は call back の略 */
static void order_cb_1(struct timer_list *t)
{
	(void)t;
	g_order[g_order_idx++] = 1;
}
static void order_cb_2(struct timer_list *t)
{
	(void)t;
	g_order[g_order_idx++] = 2;
}
static void order_cb_3(struct timer_list *t)
{
	(void)t;
	g_order[g_order_idx++] = 3;
}

/** 複数タイマーが昇順に発火することを確かめる
 * 検証対象: add_timer() の昇順挿入
 * 検証項目: expires の小さいタイマーが先に発火する
 */
static void test_timers_fire_in_order(void)
{
	struct timer_list t1, t2, t3;

	g_order_idx = 0;

	/* わざと逆順に登録する */
	timer_setup(&t3, order_cb_3);
	t3.expires = jiffies + 30;
	add_timer(&t3);

	timer_setup(&t1, order_cb_1);
	t1.expires = jiffies + 10;
	add_timer(&t1);

	timer_setup(&t2, order_cb_2);
	t2.expires = jiffies + 20;
	add_timer(&t2);

	/* 30 tick 進める */
	for (int i = 0; i < 30; i++)
	{
		jiffies++;
		run_local_timers();
	}

	/* expires が小さい順（t1→t2→t3）に発火しているはず */
	KFS_ASSERT_EQ(1, g_order[0]);
	KFS_ASSERT_EQ(2, g_order[1]);
	KFS_ASSERT_EQ(3, g_order[2]);

	printk("test_timers_fire_in_order: OK (order=%d,%d,%d)\n", g_order[0], g_order[1], g_order[2]);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_timer_fires_at_expiry, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_del_timer_cancels, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_timers_fire_in_order, setup_test, teardown_test),
};

int register_unit_tests_timer_queue(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
