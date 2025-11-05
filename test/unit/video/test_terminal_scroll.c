#include "../support/terminal_test_support.h"
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
KFS_TEST(test_scroll_up_after_full_screen)
{
	setup_terminal();

	/* 画面を満杯にする（各行を文字で満たしてから明示的に改行） */
	/* 最後の行の改行でスクロールが発生しないよう、24行までにする */
	for (int i = 0; i < KFS_VGA_HEIGHT - 1; i++)
	{
		char line_marker = 'A' + i;
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++) /* 最後の1文字は残す */
			terminal_putchar(line_marker);
		terminal_putchar('\n'); /* 明示的に改行 */
	}

	/* 25行目（Y行）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('Y');

	/* さらに1行追加してスクロールを発生させる */
	terminal_putchar('\n'); /* この改行でスクロール発生 */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('Z');
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
			terminal_putchar(line_marker);
		terminal_putchar('\n');
	}
	/* 25行目（Y行）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('Y');
	/* Z行を追加してスクロール発生 */
	terminal_putchar('\n');
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('Z');
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
			terminal_putchar(digit);
		terminal_putchar('\n');
	}
	/* 30行目（9）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('9');

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
		first_line[i] = stub[i];

	/* スクロールダウンを試みる（何も起こらないはず） */
	kfs_terminal_scroll_down();

	/* 画面内容が変わっていないことを確認 */
	for (size_t i = 0; i < KFS_VGA_WIDTH; i++)
		KFS_ASSERT_EQ((long long)first_line[i], (long long)stub[i]);
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
			terminal_putchar(letter);
		terminal_putchar('\n');
	}
	/* 25行目（Y）を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('Y');

	/* さらに10行追加してスクロールを10回発生させる（A～Jがスクロールアウト） */
	for (int i = 0; i < 10; i++)
	{
		terminal_putchar('\n');
		char letter = 'A' + ((25 + i) % 26); /* Z, A, B, C, D, E, F, G, H, I */
		for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
			terminal_putchar(letter);
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
			terminal_putchar(letter);
		terminal_putchar('\n');
	}
	/* Z行を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('Z');

	/* この時点でA行がスクロールバックに、画面はB～Zのはず */
	/* スクロールアップ */
	kfs_terminal_scroll_up();
	KFS_ASSERT_EQ('A', get_char_at(0));

	/* 新しい行を出力（スクロールを発生させる） */
	terminal_putchar('\n');
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('X');
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
			terminal_putchar('A');
		terminal_putchar('\n');
	}
	/* 25行目を改行なしで書く */
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('A');
	/* 26行目を追加してスクロール発生 */
	terminal_putchar('\n');
	for (int j = 0; j < KFS_VGA_WIDTH - 1; j++)
		terminal_putchar('A');

	kfs_terminal_scroll_up();

	/* スクロールアップ後、最初の文字は'A'のはず */
	KFS_ASSERT_EQ('A', get_char_at(0));

	/* コンソール1に切り替え */
	kfs_terminal_switch_console(1);

	/* コンソール1で出力 */
	for (int j = 0; j < 5; j++)
		terminal_putchar('B');

	/* コンソール1では'B'が表示されているはず */
	KFS_ASSERT_EQ('B', get_char_at(0));

	/* コンソール0に戻る */
	kfs_terminal_switch_console(0);

	/* コンソール0ではスクロールアップした状態が維持されているはず */
	KFS_ASSERT_EQ('A', get_char_at(0));
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_scroll_up_after_full_screen), KFS_REGISTER_TEST(test_scroll_down_returns_to_latest),
	KFS_REGISTER_TEST(test_multiple_scroll_up),			 KFS_REGISTER_TEST(test_scroll_down_at_bottom_is_noop),
	KFS_REGISTER_TEST(test_scroll_up_beyond_limit),		 KFS_REGISTER_TEST(test_new_output_resets_scroll_offset),
	KFS_REGISTER_TEST(test_scroll_on_empty_screen),		 KFS_REGISTER_TEST(test_scroll_per_console),
};

int register_host_tests_terminal_scroll(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
