#include "../support/terminal_test_support.h"
#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/console.h>
#include <kfs/printk.h>

#include <kfs/stdint.h>
#include <kfs/string.h>

static uint16_t stub[KFS_VGA_WIDTH * KFS_VGA_HEIGHT];

static inline uint16_t make_cell(char c, uint8_t color)
{
	return (uint16_t)c | ((uint16_t)color << 8);
}

/* 画面の指定位置の文字を取得（色情報は無視） */
static inline char get_char_at(size_t pos)
{
	return (char)(stub[pos] & 0xFF);
}

static void setup_terminal(void)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
}

/* スクロール機能の基本テスト: 画面が満杯になってスクロールした後、上矢印でスクロールアップできる */
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

KFS_TEST(test_scroll_up_after_full_screen)
{
	setup_terminal();

	/* 画面を満杯にする（各行を文字で満たしてから明示的に改行） */
	/* 最後の行の改行でスクロールが発生しないよう、24行までにする */
	for (int i = 0; i < KFS_VGA_HEIGHT - 1; i++)
	{
		char line_marker = 'A' + i;
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++) /* 最後の1文字は残す */
		{
			terminal_putchar(line_marker);
		}
		terminal_putchar('\n'); /* 明示的に改行 */
	}

	/* 25行目（Y行）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('Y');
	}

	/* さらに1行追加してスクロールを発生させる */
	terminal_putchar('\n'); /* この改行でスクロール発生 */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('Z');
	}
	/* Z行は改行なし - これでスクロールは1回だけ */

	/* この時点で最初の行（'A'で満たされた行）はスクロールアウトしているはず */
	/* 画面の最初の行は'B'で満たされているはず */
	KFS_ASSERT_EQ('B', get_char_at(0));

	/* スクロールアップを実行 */
	kfs_terminal_scroll_up();

	/* スクロールアップ後、最初の行に'A'が見えるはず */
	KFS_ASSERT_EQ('A', get_char_at(0));
}

/* スクロールダウンのテスト: スクロールアップした後、スクロールダウンで元に戻る */
KFS_TEST(test_scroll_down_returns_to_latest)
{
	setup_terminal();

	/* 画面を満杯にしてスクロールを1回だけ発生させる */
	for (int i = 0; i < KFS_VGA_HEIGHT - 1; i++)
	{
		char line_marker = 'A' + i;
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(line_marker);
		}
		terminal_putchar('\n');
	}
	/* 25行目（Y行）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('Y');
	}
	/* Z行を追加してスクロール発生 */
	terminal_putchar('\n');
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('Z');
	}
	/* Z行は改行なし */

	/* 'B'が最初の行のはず */
	KFS_ASSERT_EQ('B', get_char_at(0));

	/* スクロールアップ */
	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('A', get_char_at(0));

	/* スクロールダウンで元に戻る */
	kfs_terminal_scroll_down();
	KFS_ASSERT_EQ('B', get_char_at(0));
}

/* 複数回スクロールアップのテスト */
KFS_TEST(test_multiple_scroll_up)
{
	setup_terminal();

	/* 30行分出力してスクロールを発生させる（最後の行の改行を除く） */
	for (int i = 0; i < 29; i++)
	{
		char digit = '0' + (i % 10);
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(digit);
		}
		terminal_putchar('\n');
	}
	/* 30行目（9）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('9');
	}

	/* 最初の行は'5'のはず（0,1,2,3,4がスクロールアウト） */
	KFS_ASSERT_EQ('5', get_char_at(0));

	/* 2回スクロールアップ */
	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('4', get_char_at(0));

	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('3', get_char_at(0));

	/* 2回スクロールダウンで元に戻る */
	kfs_terminal_scroll_down();
	KFS_ASSERT_EQ('4', get_char_at(0));

	kfs_terminal_scroll_down();
	KFS_ASSERT_EQ('5', get_char_at(0));
}

/* スクロールダウンの境界テスト: すでに最新の状態でスクロールダウンしても何も起こらない */
KFS_TEST(test_scroll_down_at_bottom_is_noop)
{
	setup_terminal();

	terminal_writestring("Test\n");

	/* 最新の状態で内容を保存 */
	uint16_t first_line[KFS_VGA_WIDTH];
	for (size_t i = 0; i < KFS_VGA_WIDTH; i++)
	{
		first_line[i] = stub[i];
	}

	/* スクロールダウンを試みる（何も起こらないはず） */
	kfs_terminal_scroll_down();

	/* 画面内容が変わっていないことを確認 */
	for (size_t i = 0; i < KFS_VGA_WIDTH; i++)
	{
		KFS_ASSERT_EQ((long long)first_line[i], (long long)stub[i]);
	}
}

/* スクロールアップの境界テスト: スクロールバックバッファの上限を超えてスクロールアップしても安全 */
KFS_TEST(test_scroll_up_beyond_limit)
{
	setup_terminal();

	/* 10回スクロールを発生させる: 25行埋めてから10行追加 */
	/* まず24行書く（A～X） */
	for (int i = 0; i < KFS_VGA_HEIGHT - 1; i++)
	{
		char letter = 'A' + (i % 26);
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(letter);
		}
		terminal_putchar('\n');
	}
	/* 25行目（Y）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('Y');
	}

	/* さらに10行追加してスクロールを10回発生させる（A～Jがスクロールアウト） */
	for (int i = 0; i < 10; i++)
	{
		terminal_putchar('\n');
		char letter = 'A' + ((25 + i) % 26); /* Z, A, B, C, D, E, F, G, H, I */
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(letter);
		}
	}

	/* この時点で画面の最初の行は'K'のはず（A～Jがスクロールアウト） */
	KFS_ASSERT_EQ('K', get_char_at(0));

	/* 1回スクロールアップ */
	kfs_terminal_scroll_up();
	/* 最初の行は'J'のはず（スクロールバックの最新行） */
	KFS_ASSERT_EQ('J', get_char_at(0));

	/* さらにスクロールアップ */
	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('I', get_char_at(0));

	/* 最大までスクロールアップ（残り8回で合計10回） */
	for (int i = 0; i < 8; i++)
	{
		kfs_terminal_scroll_up();
	}

	/* クラッシュせず、最古の行が表示されていることを確認 */
	/* 最初の行は'A'のはず（10回スクロールでA～Jがスクロールバックに保存されている） */
	KFS_ASSERT_EQ('A', get_char_at(0));

	/* 上限を超えてスクロールアップを試みる（1回ずつ確認） */
	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('A', get_char_at(0)); /* まだ'A'のはず */

	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('A', get_char_at(0)); /* まだ'A'のはず */

	/* 残りも試みる */
	for (int i = 0; i < 8; i++)
	{
		kfs_terminal_scroll_up();
	}

	/* まだ'A'が表示されているはず（クランプされている） */
	KFS_ASSERT_EQ('A', get_char_at(0));
}

/* 新しい出力がスクロールオフセットをリセットすることをテスト */
KFS_TEST(test_new_output_resets_scroll_offset)
{
	setup_terminal();

	/* 画面を満杯にしてスクロールを1回発生させる */
	for (int i = 0; i < KFS_VGA_HEIGHT; i++)
	{
		char letter = 'A' + i;
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(letter);
		}
		terminal_putchar('\n');
	}
	/* Z行を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('Z');
	}

	/* この時点でA行がスクロールバックに、画面はB～Zのはず */
	/* スクロールアップ */
	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('A', get_char_at(0));

	/* 新しい行を出力（スクロールを発生させる） */
	terminal_putchar('\n');
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('X');
	}
	/* X行は改行なし */

	/* 自動的に最新表示にリセットされるはず */
	/* スクロールアップしていた状態が解除され、最新の内容が表示される */
	/* B行がスクロールアウト、画面はC～Xのはず */
	KFS_ASSERT_EQ('C', get_char_at(0));
}

/* 空の画面でスクロール操作をしても安全 */
KFS_TEST(test_scroll_on_empty_screen)
{
	setup_terminal();

	/* 何も出力していない状態でスクロール操作 */
	kfs_terminal_scroll_up();
	kfs_terminal_scroll_down();

	/* クラッシュしないことを確認（アサーションなしで成功すればOK） */
}

/* 仮想コンソールごとに独立したスクロール状態を持つことをテスト */
KFS_TEST(test_scroll_per_console)
{
	setup_terminal();

	/* コンソール1を初期化（前のテストの影響を排除） */
	kfs_terminal_switch_console(1);
	terminal_initialize();
	kfs_terminal_switch_console(0);

	/* コンソール0で出力してスクロールを1回発生させる */
	/* 24行書く */
	for (int i = 0; i < KFS_VGA_HEIGHT - 1; i++)
	{
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar('A');
		}
		terminal_putchar('\n');
	}
	/* 25行目を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('A');
	}
	/* 26行目を追加してスクロール発生 */
	terminal_putchar('\n');
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
	{
		terminal_putchar('A');
	}

	kfs_terminal_scroll_up();

	/* スクロールアップ後、最初の文字は'A'のはず */
	KFS_ASSERT_EQ('A', get_char_at(0));

	/* コンソール1に切り替え */
	kfs_terminal_switch_console(1);

	/* コンソール1で出力 */
	for (int j = 0; j < 5; j++)
	{
		terminal_putchar('B');
	}

	/* コンソール1では'B'が表示されているはず */
	KFS_ASSERT_EQ('B', get_char_at(0));

	/* コンソール0に戻る */
	kfs_terminal_switch_console(0);

	/* コンソール0ではスクロールアップした状態が維持されているはず */
	KFS_ASSERT_EQ('A', get_char_at(0));
}

/* 大量のスクロールでリングバッファがラップアラウンドするテスト */
KFS_TEST(test_scrollback_buffer_wraparound)
{
	setup_terminal();

	/* スクロールバックバッファの容量は100行 */
	/* 150行出力してリングバッファをラップアラウンドさせる */
	for (int i = 0; i < 150; i++)
	{
		char marker = 'A' + (i % 26);
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(marker);
		}
		terminal_putchar('\n');
	}

	/* 少しスクロールアップ */
	kfs_terminal_scroll_up();
	kfs_terminal_scroll_up();

	/* クラッシュしないことを確認（アサーションなしで成功すればOK） */
}

/* カーソル位置のクランプテスト */
KFS_TEST(test_cursor_move_clamp)
{
	setup_terminal();

	/* 範囲外の座標を指定してもクランプされることを確認 */
	kfs_terminal_move_cursor(1000, 1000);

	size_t row, col;
	kfs_terminal_get_cursor(&row, &col);

	/* VGA_HEIGHTとVGA_WIDTHの範囲内にクランプされるはず */
	KFS_ASSERT_TRUE(row < KFS_VGA_HEIGHT);
	KFS_ASSERT_TRUE(col < KFS_VGA_WIDTH);
}

/* 行末を超えた文字の折り返しテスト */
KFS_TEST(test_line_wrap_at_edge)
{
	setup_terminal();

	/* 行末まで文字を埋める */
	for (int i = 0; i < KFS_VGA_WIDTH; i++)
	{
		terminal_putchar('X');
	}

	/* さらに文字を追加（次の行に折り返されるはず） */
	terminal_putchar('Y');

	/* 2行目の最初の文字が'Y'であることを確認 */
	KFS_ASSERT_EQ('Y', get_char_at(KFS_VGA_WIDTH));
}

/* スクロール時のリングバッファ境界テスト */
KFS_TEST(test_scrollback_ring_buffer_edge)
{
	setup_terminal();

	/* ちょうど100行（スクロールバックバッファ容量）を出力 */
	for (int i = 0; i < 100; i++)
	{
		char marker = 'A' + (i % 26);
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		{
			terminal_putchar(marker);
		}
		terminal_putchar('\n');
	}

	/* 少しスクロールアップ */
	kfs_terminal_scroll_up();

	/* クラッシュしないことを確認（アサーションなしで成功すればOK） */
}

/* terminal_setcolor()関数のテスト */
KFS_TEST(test_terminal_setcolor)
{
	terminal_initialize();
	uint8_t test_color = kfs_vga_make_color(VGA_COLOR_GREEN, VGA_COLOR_BLUE);
	kfs_terminal_set_color(test_color);
	KFS_ASSERT_EQ(test_color, kfs_terminal_get_color());
}

/* terminal_putchar_overwrite()のテスト（上書きモード） */
KFS_TEST(test_terminal_putchar_overwrite)
{
	terminal_initialize();

	/* まず通常モードで文字を書き込む */
	terminal_putchar('A');
	terminal_putchar('B');
	terminal_putchar('C');

	/* カーソルを先頭に戻す */
	kfs_terminal_move_cursor(0, 0);

	/* 上書きモードで'X'を書き込む */
	terminal_putchar_overwrite('X');

	/* カーソルは移動しないことを確認 */
	size_t row, col;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, row);
	KFS_ASSERT_EQ(0, col);

	/* シャドウバッファで'X'が書かれたことを確認 */
	uint16_t *shadow = (uint16_t *)kfs_terminal_buffer;
	char written_char = (char)(shadow[0] & 0xFF);
	KFS_ASSERT_EQ('X', written_char);
}

/* terminal_delete_char()のテスト（文字削除とシフト） */
KFS_TEST(test_terminal_delete_char)
{
	terminal_initialize();

	/* "ABCDE"と書き込む */
	terminal_putchar('A');
	terminal_putchar('B');
	terminal_putchar('C');
	terminal_putchar('D');
	terminal_putchar('E');

	/* カーソルを'C'の位置(列2)に移動 */
	kfs_terminal_move_cursor(0, 2);

	/* 'C'を削除して右側をシフト */
	terminal_delete_char();

	/* シャドウバッファを確認: "ABDE " になっているはず */
	uint16_t *shadow = (uint16_t *)kfs_terminal_buffer;
	KFS_ASSERT_EQ('A', (char)(shadow[0] & 0xFF));
	KFS_ASSERT_EQ('B', (char)(shadow[1] & 0xFF));
	KFS_ASSERT_EQ('D', (char)(shadow[2] & 0xFF));
	KFS_ASSERT_EQ('E', (char)(shadow[3] & 0xFF));
	KFS_ASSERT_EQ(' ', (char)(shadow[4] & 0xFF));
}

/* kfs_terminal_get_color()とkfs_terminal_active_console()のテスト */
KFS_TEST(test_terminal_get_color_and_active_console)
{
	terminal_initialize();

	/* 色設定とget_color()のテスト */
	uint8_t test_color = kfs_vga_make_color(VGA_COLOR_RED, VGA_COLOR_WHITE);
	kfs_terminal_set_color(test_color);
	KFS_ASSERT_EQ(test_color, kfs_terminal_get_color());

	/* active_console()のテスト - 初期状態ではコンソール0がアクティブ */
	KFS_ASSERT_EQ(0, kfs_terminal_active_console());

	/* コンソール1に切り替えて確認 */
	kfs_terminal_switch_console(1);
	KFS_ASSERT_EQ(1, kfs_terminal_active_console());

	/* コンソール0に戻す */
	kfs_terminal_switch_console(0);
	KFS_ASSERT_EQ(0, kfs_terminal_active_console());
}

/* kfs_terminal_cursor_left()のテスト（左矢印キー） */
KFS_TEST(test_cursor_left_movement)
{
	terminal_initialize();

	/* カーソルを(0, 5)に移動 */
	kfs_terminal_move_cursor(0, 5);

	/* 左に移動 */
	kfs_terminal_cursor_left();

	size_t row, col;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, row);
	KFS_ASSERT_EQ(4, col);

	/* カーソルを(1, 0)に移動（行頭） */
	kfs_terminal_move_cursor(1, 0);

	/* 左に移動 → 前の行の末尾(0, VGA_WIDTH-1)に移動するはず */
	kfs_terminal_cursor_left();

	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, row);
	KFS_ASSERT_EQ(79, col); /* VGA_WIDTH-1 = 79 */

	/* スクロール中は左矢印キーが無効になることを確認 */
	kfs_terminal_move_cursor(5, 10);

	/* 何行か書き込んでスクロールバックに保存 */
	for (int i = 0; i < 30; i++)
	{
		terminal_writestring("Line\n");
	}

	/* スクロールアップ */
	kfs_terminal_scroll_up();

	/* カーソル移動を試みる（無効化されているはず） */
	size_t row_before, col_before;
	kfs_terminal_get_cursor(&row_before, &col_before);

	kfs_terminal_cursor_left();

	size_t row_after, col_after;
	kfs_terminal_get_cursor(&row_after, &col_after);

	/* スクロール中なのでカーソル位置は変わらないはず */
	KFS_ASSERT_EQ(row_before, row_after);
	KFS_ASSERT_EQ(col_before, col_after);
}

/* kfs_terminal_cursor_right()で行末から次行へ移動するテスト */
KFS_TEST(test_cursor_right_wrap_to_next_line)
{
	terminal_initialize();

	/* カーソルを行末(0, VGA_WIDTH-1)に移動 */
	kfs_terminal_move_cursor(0, 79); /* VGA_WIDTH-1 */

	/* 右に移動 → 次の行の先頭(1, 0)に移動するはず */
	kfs_terminal_cursor_right();

	size_t row, col;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(1, row);
	KFS_ASSERT_EQ(0, col);

	/* 通常の右移動もテスト */
	kfs_terminal_move_cursor(5, 10);
	kfs_terminal_cursor_right();

	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(5, row);
	KFS_ASSERT_EQ(11, col);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_scroll_up_after_full_screen, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scroll_down_returns_to_latest, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_multiple_scroll_up, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scroll_down_at_bottom_is_noop, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scroll_up_beyond_limit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_new_output_resets_scroll_offset, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scroll_on_empty_screen, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scroll_per_console, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scrollback_buffer_wraparound, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cursor_move_clamp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_line_wrap_at_edge, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_scrollback_ring_buffer_edge, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_terminal_setcolor, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_terminal_putchar_overwrite, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_terminal_delete_char, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_terminal_get_color_and_active_console, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cursor_left_movement, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cursor_right_wrap_to_next_line, setup_test, teardown_test),
};

int register_host_tests_terminal_scroll(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
