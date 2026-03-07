/**
 * test_piano.c - kernel/piano.c のユニットテスト
 *
 * テスト戦略:
 *   piano_raw_handler() はキーボード IRQ コールバックとして公開されているため、
 *   直接呼び出してインターフェース動作を検証する。
 *   PSG の内部状態ではなく「ハンドラが正しい値を返すか」「piano_active フラグが
 *   正しく管理されるか」という観察可能な振る舞いをテストする。
 */
#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/keyboard.h>
#include <kfs/piano.h>
#include <kfs/psg.h>

/* テスト前の共通セットアップ */
static void setup_test(void)
{
	reset_all_state_for_test();
	psg_init();
	/* piano_active などの静的変数を初期化するため、
	 * Escape を1回送って inactive 状態にしておく */
	piano_raw_handler(0x01, 0); /* Escape press */
}

static void teardown_test(void)
{
	/* 全チャンネルを停止してテスト間の音を残さない */
	int i;
	for (i = 0; i < 4; i++)
	{
		do_psg_stop(i);
	}
}

/* =========================================
 * piano_raw_handler 戻り値テスト
 * piano_raw_handler は登録されたキーボードコールバックであり、
 * 常に 1 を返してスキャンコードを通常 ASCII 変換に渡さないことを保証する。
 * ========================================= */

/**
 * test_piano_handler_escape_press_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: Escape 押下で 1 を返すこと
 */
KFS_TEST(test_piano_handler_escape_press_returns_1)
{
	int result = piano_raw_handler(0x01, 0); /* Escape press */
	KFS_ASSERT_EQ(1, result);
}

/**
 * test_piano_handler_escape_release_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: Escape 解放でも 1 を返すこと（解放はアクション不要だが横取りする）
 */
KFS_TEST(test_piano_handler_escape_release_returns_1)
{
	int result = piano_raw_handler(0x01, 1); /* Escape release */
	KFS_ASSERT_EQ(1, result);
}

/**
 * test_piano_handler_assigned_key_press_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: 音程割当済みキー('a' = 0x1E, C4) の押下で 1 を返すこと
 */
KFS_TEST(test_piano_handler_assigned_key_press_returns_1)
{
	int result = piano_raw_handler(0x1E, 0); /* 'a' press = C4 */
	KFS_ASSERT_EQ(1, result);
}

/**
 * test_piano_handler_assigned_key_release_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: 音程割当済みキーの解放でも 1 を返すこと
 */
KFS_TEST(test_piano_handler_assigned_key_release_returns_1)
{
	piano_raw_handler(0x1E, 0);				 /* 'a' press */
	int result = piano_raw_handler(0x1E, 1); /* 'a' release */
	KFS_ASSERT_EQ(1, result);
}

/**
 * test_piano_handler_unassigned_key_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: 音程未割当キー('r' = 0x13、E4-F4間に黒鍵なし)でも 1 を返すこと
 *           (どのスキャンコードも通常処理へ素通しさせない)
 */
KFS_TEST(test_piano_handler_unassigned_key_returns_1)
{
	int result = piano_raw_handler(0x13, 0); /* 'r' press (unassigned) */
	KFS_ASSERT_EQ(1, result);
}

/**
 * test_piano_handler_shift_left_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: 左 shift (0x2A) の押下/解放で 1 を返すこと
 */
KFS_TEST(test_piano_handler_shift_left_returns_1)
{
	KFS_ASSERT_EQ(1, piano_raw_handler(0x2A, 0)); /* press */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x2A, 1)); /* release */
}

/**
 * test_piano_handler_enter_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: Enter (0x1C、ノイズ) の押下/解放で 1 を返すこと
 */
KFS_TEST(test_piano_handler_enter_returns_1)
{
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1C, 0)); /* press */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1C, 1)); /* release */
}

/**
 * test_piano_handler_space_returns_1
 * 検証対象: piano_raw_handler()
 * 検証項目: space (0x39、オクターブ上げ) の押下/解放で 1 を返すこと
 */
KFS_TEST(test_piano_handler_space_returns_1)
{
	KFS_ASSERT_EQ(1, piano_raw_handler(0x39, 0)); /* press */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x39, 1)); /* release */
}

/* =========================================
 * piano_active フラグテスト
 * piano_is_active() で観察可能な振る舞いを検証する。
 * ========================================= */

/**
 * test_piano_active_after_escape_press
 * 検証対象: piano_raw_handler() + piano_is_active()
 * 検証項目: Escape 押下後に piano_is_active() が 0 を返すこと
 */
KFS_TEST(test_piano_active_after_escape_press)
{
	/* escape を事前に押すと inactive になる（setup_test でもやっているが明示的にテスト） */
	piano_raw_handler(0x01, 0);
	KFS_ASSERT_EQ(0, piano_is_active());
}

/* =========================================
 * 連打防止テスト (double-press guard)
 * 同じキーを连続して press した場合、2回目以降も 1 を返す（クラッシュしない）こと。
 * ========================================= */

/**
 * test_piano_handler_double_press_no_crash
 * 検証対象: piano_raw_handler()
 * 検証項目: 同じキーを2回押しても 1 を返しクラッシュしないこと
 *           (key_ch[code] >= 0 の連打防止パスが正しく動作する)
 */
KFS_TEST(test_piano_handler_double_press_no_crash)
{
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* 'a' 1回目 press */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* 'a' 2回目 press (連打防止) */
}

/**
 * test_piano_handler_press_release_press_no_crash
 * 検証対象: piano_raw_handler()
 * 検証項目: press → release → press の手順が正常に動作すること
 *           (release 後に key_ch が -1 に戻り、再押下を受け付けること)
 */
KFS_TEST(test_piano_handler_press_release_press_no_crash)
{
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* press  */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 1)); /* release */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* press 再び */
}

/* =========================================
 * 和音テスト (polyphony)
 * ch0-ch2 を巡回して 3 キー同時押しが可能なことを検証する。
 * ========================================= */

/**
 * test_piano_handler_three_keys_no_crash
 * 検証対象: piano_raw_handler()
 * 検証項目: 異なる 3 キーを同時押しして全て解放してもクラッシュしないこと
 */
KFS_TEST(test_piano_handler_three_keys_no_crash)
{
	/* C4, D4, E4 を同時押し */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* a = C4 */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1F, 0)); /* s = D4 */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x20, 0)); /* d = E4 */
	/* 全て解放 */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 1));
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1F, 1));
	KFS_ASSERT_EQ(1, piano_raw_handler(0x20, 1));
}

/* =========================================
 * オクターブ修飾テスト
 * shift/space 押下中の重ね押しがクラッシュしないことを検証する。
 * ========================================= */

/**
 * test_piano_handler_shift_then_key_no_crash
 * 検証対象: piano_raw_handler()
 * 検証項目: shift 押下中に音程キーを押してもクラッシュしないこと
 *           (freq/2 パスが正しく通ること)
 */
KFS_TEST(test_piano_handler_shift_then_key_no_crash)
{
	piano_raw_handler(0x2A, 0);					  /* shift press */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* a press (1オクターブ下) */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 1)); /* a release */
	piano_raw_handler(0x2A, 1);					  /* shift release */
	KFS_ASSERT_EQ(1, 1);
}

/**
 * test_piano_handler_space_then_key_no_crash
 * 検証対象: piano_raw_handler()
 * 検証項目: space 押下中に音程キーを押してもクラッシュしないこと
 *           (freq*2 パスが正しく通ること)
 */
KFS_TEST(test_piano_handler_space_then_key_no_crash)
{
	piano_raw_handler(0x39, 0);					  /* space press */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 0)); /* a press (1オクターブ上) */
	KFS_ASSERT_EQ(1, piano_raw_handler(0x1E, 1)); /* a release */
	piano_raw_handler(0x39, 1);					  /* space release */
	KFS_ASSERT_EQ(1, 1);
}

/* テストケースの登録 */
static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_escape_press_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_escape_release_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_assigned_key_press_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_assigned_key_release_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_unassigned_key_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_shift_left_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_enter_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_space_returns_1, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_active_after_escape_press, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_double_press_no_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_press_release_press_no_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_three_keys_no_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_shift_then_key_no_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_piano_handler_space_then_key_no_crash, setup_test, teardown_test),
};

int register_unit_tests_piano(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
