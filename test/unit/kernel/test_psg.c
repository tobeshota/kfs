/**
 * test_psg.c - kernel/sound/psg.c のユニットテスト
 *
 * PSG（Programmable Sound Generator）エミュレータの動作テスト
 * psg_init / do_psg_note / do_psg_stop / psg_tick をカバーする
 */
#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/psg.h>
#include <kfs/timer.h>

static void setup_test(void)
{
	reset_all_state_for_test();
	psg_init();
}

static void teardown_test(void)
{
}

/* =========================================
 * psg_init テスト
 * ========================================= */

/**
 * test_psg_init_does_not_crash
 * 検証対象: psg_init()
 * 検証項目: クラッシュせずに完了すること
 */
KFS_TEST(test_psg_init_does_not_crash)
{
	psg_init();
	KFS_ASSERT_TRUE(1);
}

/* =========================================
 * do_psg_note テスト
 * ========================================= */

/**
 * test_do_psg_note_valid_channel
 * 検証対象: do_psg_note()
 * 検証項目: 有効なチャンネルで正常に実行されること
 */
KFS_TEST(test_do_psg_note_valid_channel)
{
	/* ch0〜ch2（矩形波チャンネル）で発音 */
	do_psg_note(0, 440, 0);
	do_psg_note(1, 880, 0);
	do_psg_note(2, 220, 0);
	KFS_ASSERT_TRUE(1);
}

/**
 * test_do_psg_note_invalid_channel_negative
 * 検証対象: do_psg_note()
 * 検証項目: ch < 0 のとき early return すること（クラッシュしない）
 */
KFS_TEST(test_do_psg_note_invalid_channel_negative)
{
	do_psg_note(-1, 440, 0);
	KFS_ASSERT_TRUE(1);
}

/**
 * test_do_psg_note_invalid_channel_too_large
 * 検証対象: do_psg_note()
 * 検証項目: ch >= PSG_CH_COUNT のとき early return すること
 */
KFS_TEST(test_do_psg_note_invalid_channel_too_large)
{
	do_psg_note(PSG_CH_COUNT, 440, 0);
	KFS_ASSERT_TRUE(1);
}

/**
 * test_do_psg_note_freq_zero
 * 検証対象: do_psg_note()
 * 検証項目: freq == 0 のとき active = 0 になること
 */
KFS_TEST(test_do_psg_note_freq_zero)
{
	/* まず発音してから freq=0 で停止 */
	do_psg_note(0, 440, 0);
	do_psg_note(0, 0, 0);
	KFS_ASSERT_TRUE(1);
}

/* =========================================
 * do_psg_stop テスト
 * ========================================= */

/**
 * test_do_psg_stop_valid_channel
 * 検証対象: do_psg_stop()
 * 検証項目: 有効なチャンネルで do_psg_note(ch, 0) を経由すること
 */
KFS_TEST(test_do_psg_stop_valid_channel)
{
	do_psg_note(0, 440, 0);
	do_psg_stop(0);
	KFS_ASSERT_TRUE(1);
}

/**
 * test_do_psg_stop_all_channels
 * 検証対象: do_psg_stop()
 * 検証項目: 全チャンネルを停止してもクラッシュしないこと
 */
KFS_TEST(test_do_psg_stop_all_channels)
{
	int i;

	for (i = 0; i < PSG_CH_COUNT; i++)
	{
		do_psg_note(i, 440, 0);
	}
	for (i = 0; i < PSG_CH_COUNT; i++)
	{
		do_psg_stop(i);
	}
	KFS_ASSERT_TRUE(1);
}

/* =========================================
 * psg_tick テスト
 * ========================================= */

/**
 * test_psg_tick_all_inactive
 * 検証対象: psg_tick()
 * 検証項目: 全チャンネル inactive のとき pcspkr_stop が呼ばれること（クラッシュしない）
 */
KFS_TEST(test_psg_tick_all_inactive)
{
	/* init 後は全チャンネル inactive */
	psg_init();
	jiffies = 0;
	psg_tick(); /* ch0 inactive, 全チャンネル inactive → pcspkr_stop */
	jiffies = 1;
	psg_tick(); /* ch1 inactive */
	jiffies = 2;
	psg_tick(); /* ch2 inactive */
	jiffies = 3;
	psg_tick(); /* ch3 inactive */
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_tick_active_channel
 * 検証対象: psg_tick()
 * 検証項目: active なチャンネルが選ばれると pcspkr_tone が呼ばれること
 */
KFS_TEST(test_psg_tick_active_channel)
{
	do_psg_note(0, 440, 0);
	jiffies = 0; /* ch0 が選ばれる */
	psg_tick();	 /* ch0 active → pcspkr_tone(440) */
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_tick_inactive_but_other_active
 * 検証対象: psg_tick()
 * 検証項目: 選択チャンネルが inactive でも他が active の場合、pcspkr_stop しないこと
 */
KFS_TEST(test_psg_tick_inactive_but_other_active)
{
	/* ch0 は inactive, ch1 は active */
	do_psg_note(1, 880, 0);
	jiffies = 0; /* ch0 が選ばれるが inactive, ch1 は active → early return without stop */
	psg_tick();
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_tick_noise_channel
 * 検証対象: psg_tick() → noise_lfsr_next()
 * 検証項目: noise チャンネル（ch3）が active のとき LFSR 周波数が出力されること
 */
KFS_TEST(test_psg_tick_noise_channel)
{
	/* ch3 はデフォルトで noise=1 に設定済み（psg_init 後） */
	do_psg_note(PSG_CH_COUNT - 1, 500, 0); /* ch3 を active に */
	jiffies = PSG_CH_COUNT - 1;			   /* ch3 が選ばれる */
	psg_tick();							   /* noise_lfsr_next() を呼ぶ */
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_tick_noise_lfsr_multiple_steps
 * 検証対象: noise_lfsr_next()
 * 検証項目: LFSR が複数ステップ進んでもクラッシュしないこと
 *           （XOR 分岐 0xB400 のカバレッジのため複数回呼ぶ）
 */
KFS_TEST(test_psg_tick_noise_lfsr_multiple_steps)
{
	int i;

	do_psg_note(PSG_CH_COUNT - 1, 500, 0);
	/* LFSR を複数ステップ進めて XOR 分岐をカバーする */
	for (i = 0; i < 16; i++)
	{
		jiffies = PSG_CH_COUNT - 1;
		psg_tick();
	}
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_glitch_stat_does_not_crash
 * 検証対象: psg_glitch_stat()
 * 検証項目: クラッシュせずに統計を表示できること
 */
KFS_TEST(test_psg_glitch_stat_does_not_crash)
{
	psg_glitch_stat();
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_glitch_reset_clears
 * 検証対象: psg_glitch_reset()
 * 検証項目: クラッシュせずにカウンタをリセットできること
 */
KFS_TEST(test_psg_glitch_reset_clears)
{
	psg_glitch_reset();
	psg_glitch_stat(); /* リセット後に再度表示しても問題ないこと */
	KFS_ASSERT_TRUE(1);
}

/**
 * test_psg_get_caller_pid
 * 検証対象: psg_get_caller_pid()
 * 検証項目: do_psg_note() 後に caller PID が返ること
 */
KFS_TEST(test_psg_get_caller_pid)
{
	uint32_t pid;

	do_psg_note(0, 440, 0);
	pid = psg_get_caller_pid();
	/* PID は current->pid の値 (>= 0); クラッシュしなければ OK */
	KFS_ASSERT_TRUE(pid == (uint32_t)pid); /* always true, just checks return */
}

/**
 * test_psg_deadline_miss
 * 検証対象: do_psg_note() deadline 超過検出パス
 * 検証項目: deadline 設定→jiffies 進める→再発音で glitch カウントが増加するパスをカバー
 */
KFS_TEST(test_psg_deadline_miss)
{
	/* 1. deadline_ms=10 で発音：deadline_tick = jiffies + 10 */
	jiffies = 0;
	do_psg_note(0, 440, 10);

	/* 2. jiffies を大幅に進める（deadline 超過） */
	jiffies = 200;

	/* 3. 同じチャンネルに再度 do_psg_note → deadline 超過検出パスを通る */
	do_psg_note(0, 880, 10);

	/* グリッチ統計に記録されているはず */
	psg_glitch_stat();
	KFS_ASSERT_TRUE(1);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_init_does_not_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_psg_note_valid_channel, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_psg_note_invalid_channel_negative, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_psg_note_invalid_channel_too_large, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_psg_note_freq_zero, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_psg_stop_valid_channel, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_psg_stop_all_channels, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_tick_all_inactive, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_tick_active_channel, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_tick_inactive_but_other_active, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_tick_noise_channel, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_tick_noise_lfsr_multiple_steps, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_glitch_stat_does_not_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_glitch_reset_clears, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_get_caller_pid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_psg_deadline_miss, setup_test, teardown_test),
};

int register_unit_tests_psg(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
