#include "../test_reset.h"
#include "host_test_framework.h"

#include <kfs/console.h>
#include <kfs/keyboard.h>

/* テスト用のカスタムハンドラで受け取った文字を記録 */
static char last_char_received;
static int handler_called;

static int test_keyboard_handler(char c)
{
	last_char_received = c;
	handler_called = 1;
	return 1; /* 処理したことを示す */
}

static void reset_handler_state(void)
{
	last_char_received = 0;
	handler_called = 0;
}

/* 通常の文字入力テスト */
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

KFS_TEST(test_keyboard_printable_chars)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* 'a' キー (scancode 0x1E) */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'a');

	reset_handler_state();

	/* 'h' キー (scancode 0x23) */
	kfs_keyboard_feed_scancode(0x23);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'h');
}

/* Shift キー + 文字入力テスト */
KFS_TEST(test_keyboard_shift_modifier)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* 左Shiftキー押下 (0x2A) */
	kfs_keyboard_feed_scancode(0x2A);
	KFS_ASSERT_EQ(0, handler_called); /* Shift単体では文字は発生しない */

	/* Shift + 'a' = 'A' */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'A');

	reset_handler_state();

	/* 左Shiftキー解放 (0x2A | 0x80 = 0xAA) */
	kfs_keyboard_feed_scancode(0xAA);

	/* 通常の 'a' */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'a');
}

/* 右Shiftキーテスト */
KFS_TEST(test_keyboard_right_shift)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* 右Shiftキー押下 (0x36) */
	kfs_keyboard_feed_scancode(0x36);

	/* Shift + 'b' = 'B' */
	kfs_keyboard_feed_scancode(0x30); /* 'b' */
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'B');

	reset_handler_state();

	/* 右Shiftキー解放 */
	kfs_keyboard_feed_scancode(0xB6);

	/* 通常の 'b' */
	kfs_keyboard_feed_scancode(0x30);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'b');
}

/* CapsLock テスト */
KFS_TEST(test_keyboard_caps_lock)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* CapsLock押下 (0x3A) */
	kfs_keyboard_feed_scancode(0x3A);

	/* CapsLockオン状態で 'a' = 'A' */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'A');

	reset_handler_state();

	/* CapsLock解放 */
	kfs_keyboard_feed_scancode(0xBA);

	/* まだCapsLockオン状態 (トグル) */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'A');

	reset_handler_state();

	/* CapsLock再押下でオフ */
	kfs_keyboard_feed_scancode(0x3A);
	kfs_keyboard_feed_scancode(0xBA);

	/* 通常の 'a' */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'a');
}

/* CapsLock + Shift テスト (反転動作) */
KFS_TEST(test_keyboard_caps_lock_with_shift)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* CapsLockオン */
	kfs_keyboard_feed_scancode(0x3A);

	/* Shiftキー押下 */
	kfs_keyboard_feed_scancode(0x2A);

	/* CapsLock + Shift で 'a' (反転して小文字) */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'a');
}

/* Backspace キーテスト */
KFS_TEST(test_keyboard_backspace)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* Backspace (0x0E) */
	kfs_keyboard_feed_scancode(0x0E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '\b');
}

/* Enter キーテスト */
KFS_TEST(test_keyboard_enter)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* Enter (0x1C) */
	kfs_keyboard_feed_scancode(0x1C);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '\n');
}

/* 拡張コード (0xE0) + 矢印キーテスト */
KFS_TEST(test_keyboard_arrow_keys)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* 上矢印: 0xE0, 0x48 */
	kfs_keyboard_feed_scancode(0xE0);
	KFS_ASSERT_EQ(0, handler_called); /* プレフィックスだけでは何も起きない */
	kfs_keyboard_feed_scancode(0x48);
	/* 上矢印はスクロールアップなので、ハンドラは呼ばれない */
	KFS_ASSERT_EQ(0, handler_called);

	reset_handler_state();

	/* 下矢印: 0xE0, 0x50 */
	kfs_keyboard_feed_scancode(0xE0);
	kfs_keyboard_feed_scancode(0x50);
	KFS_ASSERT_EQ(0, handler_called);

	reset_handler_state();

	/* 左矢印: 0xE0, 0x4B */
	kfs_keyboard_feed_scancode(0xE0);
	kfs_keyboard_feed_scancode(0x4B);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '\x1C');

	reset_handler_state();

	/* 右矢印: 0xE0, 0x4D */
	kfs_keyboard_feed_scancode(0xE0);
	kfs_keyboard_feed_scancode(0x4D);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '\x1D');
}

/* Alt + Fキー (コンソール切り替え) テスト */
KFS_TEST(test_keyboard_alt_f_keys)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();

	/* Alt押下 (0x38) */
	kfs_keyboard_feed_scancode(0x38);

	/* F1 (0x3B) */
	kfs_keyboard_feed_scancode(0x3B);
	/* コンソール0に切り替わる (実際の動作確認は困難だがクラッシュしないことを確認) */

	/* Alt解放 */
	kfs_keyboard_feed_scancode(0xB8);
}

/* 数字キーテスト */
KFS_TEST(test_keyboard_digit_keys)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* '1' キー (0x02) */
	kfs_keyboard_feed_scancode(0x02);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '1');

	reset_handler_state();

	/* Shift + '1' = '!' */
	kfs_keyboard_feed_scancode(0x2A); /* Left Shift */
	kfs_keyboard_feed_scancode(0x02);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '!');
}

/* スペースキーテスト */
KFS_TEST(test_keyboard_space)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* Space (0x39) */
	kfs_keyboard_feed_scancode(0x39);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, ' ');
}

/* タブキーテスト */
KFS_TEST(test_keyboard_tab)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* Tab (0x0F) */
	kfs_keyboard_feed_scancode(0x0F);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '\t');
}

/* キーリリースイベントのテスト */
KFS_TEST(test_keyboard_key_release)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* 'a' 押下 (0x1E) */
	kfs_keyboard_feed_scancode(0x1E);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, 'a');

	reset_handler_state();

	/* 'a' 解放 (0x1E | 0x80 = 0x9E) */
	kfs_keyboard_feed_scancode(0x9E);
	/* 解放イベントでは文字は生成されない */
	KFS_ASSERT_EQ(0, handler_called);
}

/* 記号キーのテスト */
KFS_TEST(test_keyboard_symbol_keys)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* '-' (0x0C) */
	kfs_keyboard_feed_scancode(0x0C);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '-');

	reset_handler_state();

	/* Shift + '-' = '_' */
	kfs_keyboard_feed_scancode(0x2A);
	kfs_keyboard_feed_scancode(0x0C);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '_');

	reset_handler_state();
	kfs_keyboard_feed_scancode(0xAA); /* Shift解放 */

	/* '=' (0x0D) */
	kfs_keyboard_feed_scancode(0x0D);
	KFS_ASSERT_TRUE(handler_called);
	KFS_ASSERT_EQ(last_char_received, '=');
}

/* 0xE1プレフィックステスト (Pause/Break) */
KFS_TEST(test_keyboard_e1_prefix)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	kfs_keyboard_set_handler(test_keyboard_handler);

	reset_handler_state();

	/* 0xE1プレフィックス */
	kfs_keyboard_feed_scancode(0xE1);
	KFS_ASSERT_EQ(0, handler_called);

	/* 次のコード (無視される) */
	kfs_keyboard_feed_scancode(0x1D);
	KFS_ASSERT_EQ(0, handler_called);
}

/* ハンドラなしでの動作テスト */
KFS_TEST(test_keyboard_without_handler)
{
	kfs_keyboard_reset();
	kfs_keyboard_init();
	/* ハンドラを設定しない */

	/* 通常の文字入力 (printk経由で出力される) */
	kfs_keyboard_feed_scancode(0x1E); /* 'a' */
									  /* クラッシュしないことを確認 */
}

int register_host_tests_keyboard(struct kfs_test_case **out_cases)
{
	static struct kfs_test_case cases[] = {
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_printable_chars, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_shift_modifier, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_right_shift, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_caps_lock, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_caps_lock_with_shift, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_backspace, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_enter, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_arrow_keys, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_alt_f_keys, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_digit_keys, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_space, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_tab, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_key_release, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_symbol_keys, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_e1_prefix, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_keyboard_without_handler, setup_test, teardown_test),
	};
	*out_cases = cases;
	return sizeof(cases) / sizeof(cases[0]);
}
