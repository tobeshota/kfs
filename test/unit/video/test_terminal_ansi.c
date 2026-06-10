#include "../support/terminal_test_support.h"
#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/console.h>
#include <kfs/string.h>
#include <video/vga.h>

static uint16_t stub[KFS_VGA_WIDTH * KFS_VGA_HEIGHT];

/* 指定した位置のセルの文字を取得する */
static inline char cell_char(size_t pos)
{
	return (char)(stub[pos] & 0xFF);
}

/* 指定した位置のセルの色属性を取得する */
static inline uint8_t cell_color(size_t pos)
{
	return (uint8_t)(stub[pos] >> 8);
}

/* ターミナルを初期化する */
static void setup_terminal(void)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
}

static void setup_test(void)
{
	reset_all_state_for_test();
}

static void teardown_test(void)
{
}

/* ESC[31mで前景色が赤になり、ESC[0mでデフォルト色に戻ることを確かめる */
KFS_TEST(test_ansi_sgr_fg_and_reset)
{
	setup_terminal();
	terminal_write("\x1b[31mR\x1b[0mN", strlen("\x1b[31mR\x1b[0mN"));

	KFS_ASSERT_EQ('R', cell_char(0));
	KFS_ASSERT_EQ((long long)kfs_vga_make_color(VGA_COLOR_RED, VGA_COLOR_BLACK), (long long)cell_color(0));

	KFS_ASSERT_EQ('N', cell_char(1));
	KFS_ASSERT_EQ((long long)kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), (long long)cell_color(1));
}

/* ESC[1;34;47mで太字・青前景・白背景が同時に反映されることを確かめる */
KFS_TEST(test_ansi_sgr_bold_and_bg)
{
	setup_terminal();
	terminal_write("\x1b[1;34;47mB", strlen("\x1b[1;34;47mB"));

	KFS_ASSERT_EQ('B', cell_char(0));
	KFS_ASSERT_EQ((long long)kfs_vga_make_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_GREY), (long long)cell_color(0));
}

KFS_TEST(test_terminal_tab_expands_to_next_tab_stop)
{
	setup_terminal();
	terminal_write("A\tB", strlen("A\tB"));

	KFS_ASSERT_EQ('A', cell_char(0));
	for (size_t i = 1; i < 8; i++)
	{
		KFS_ASSERT_EQ(' ', cell_char(i));
	}
	KFS_ASSERT_EQ('B', cell_char(8));
}

KFS_TEST(test_terminal_tab_uses_current_column)
{
	setup_terminal();
	terminal_write("123456\tX", strlen("123456\tX"));

	KFS_ASSERT_EQ('1', cell_char(0));
	KFS_ASSERT_EQ('6', cell_char(5));
	KFS_ASSERT_EQ(' ', cell_char(6));
	KFS_ASSERT_EQ(' ', cell_char(7));
	KFS_ASSERT_EQ('X', cell_char(8));
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_ansi_sgr_fg_and_reset, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_ansi_sgr_bold_and_bg, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_terminal_tab_expands_to_next_tab_stop, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_terminal_tab_uses_current_column, setup_test, teardown_test),
};

int register_unit_tests_terminal_ansi(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
