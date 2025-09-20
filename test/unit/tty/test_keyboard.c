#include "../support/io_stub_control.h"
#include "../support/terminal_test_support.h"
#include "host_test_framework.h"
#include <linux/console.h>
#include <linux/keyboard.h>
#include <linux/printk.h>

static uint16_t stub[KFS_VGA_WIDTH * KFS_VGA_HEIGHT];

static inline uint16_t make_cell(char c, uint8_t color)
{
	return (uint16_t)c | ((uint16_t)color << 8);
}

static void setup_keyboard_common(int call_init)
{
	for (size_t i = 0; i < KFS_VGA_WIDTH * KFS_VGA_HEIGHT; i++)
		stub[i] = 0;
	kfs_stub_reset_io();
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	for (size_t i = 1; i < KFS_VIRTUAL_CONSOLE_COUNT; i++)
	{
		kfs_terminal_switch_console(i);
		terminal_initialize();
	}
	kfs_terminal_switch_console(0);
	if (call_init)
	{
		kfs_keyboard_init();
	}
	else
	{
		kfs_keyboard_reset();
	}
}

static void setup_keyboard(void)
{
	setup_keyboard_common(1);
}

KFS_TEST(test_keyboard_letters_shift_caps)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x1E); /* a */
	kfs_keyboard_feed_scancode(0x9E);
	kfs_keyboard_feed_scancode(0x2A); /* shift */
	kfs_keyboard_feed_scancode(0x1E);
	kfs_keyboard_feed_scancode(0x9E);
	kfs_keyboard_feed_scancode(0xAA);
	kfs_keyboard_feed_scancode(0x3A); /* caps lock */
	kfs_keyboard_feed_scancode(0x1E);
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('a', color), (long long)stub[0]);
	KFS_ASSERT_EQ((long long)make_cell('A', color), (long long)stub[1]);
	KFS_ASSERT_EQ((long long)make_cell('A', color), (long long)stub[2]);
}

KFS_TEST(test_keyboard_digits_and_symbols)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x02); /* 1 */
	kfs_keyboard_feed_scancode(0x82);
	kfs_keyboard_feed_scancode(0x2A); /* shift */
	kfs_keyboard_feed_scancode(0x02);
	kfs_keyboard_feed_scancode(0x82);
	kfs_keyboard_feed_scancode(0xAA);
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('1', color), (long long)stub[0]);
	KFS_ASSERT_EQ((long long)make_cell('!', color), (long long)stub[1]);
}

KFS_TEST(test_keyboard_right_shift_uppercase)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x36);
	kfs_keyboard_feed_scancode(0x10); /* q -> uppercase with right shift */
	kfs_keyboard_feed_scancode(0xB6);
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('Q', color), (long long)stub[0]);
}

KFS_TEST(test_keyboard_backspace)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x1E); /* a */
	kfs_keyboard_feed_scancode(0x30); /* b */
	kfs_keyboard_feed_scancode(0x2E); /* c */
	kfs_keyboard_feed_scancode(0x0E); /* backspace */
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)2, (long long)col);
	KFS_ASSERT_EQ((long long)make_cell(' ', color), (long long)stub[2]);
}

KFS_TEST(test_keyboard_backspace_previous_row)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x1C); /* enter */
	kfs_keyboard_feed_scancode(0x9C);
	kfs_keyboard_feed_scancode(0x0E); /* backspace when row=1 col=0 */
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ(0, (long long)row);
	KFS_ASSERT_EQ((long long)(KFS_VGA_WIDTH - 1), (long long)col);
	KFS_ASSERT_EQ((long long)make_cell(' ', color), (long long)stub[(KFS_VGA_WIDTH - 1)]);
}

KFS_TEST(test_keyboard_alt_function_switch_console)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x0B); /* 0 */
	kfs_keyboard_feed_scancode(0x8B);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
	kfs_keyboard_feed_scancode(0x38); /* alt down */
	kfs_keyboard_feed_scancode(0x3C); /* F2 */
	kfs_keyboard_feed_scancode(0xBC);
	kfs_keyboard_feed_scancode(0xB8);
	KFS_ASSERT_EQ(1, (long long)kfs_terminal_active_console());
	kfs_keyboard_feed_scancode(0x02); /* 1 */
	kfs_keyboard_feed_scancode(0x82);
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('1', color), (long long)stub[0]);
	kfs_keyboard_feed_scancode(0x38);
	kfs_keyboard_feed_scancode(0x3B);
	kfs_keyboard_feed_scancode(0xBB);
	kfs_keyboard_feed_scancode(0xB8);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
	KFS_ASSERT_EQ((long long)make_cell('0', color), (long long)stub[0]);
}

KFS_TEST(test_keyboard_extended_sequence_ignored)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0xE0);
	kfs_keyboard_feed_scancode(0x50);
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, (long long)row);
	KFS_ASSERT_EQ(0, (long long)col);
}

KFS_TEST(test_keyboard_unknown_key_is_ignored)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x3F); /* F5 (no mapping) */
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, (long long)row);
	KFS_ASSERT_EQ(0, (long long)col);
}

KFS_TEST(test_keyboard_shift_nonmapped_falls_back)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x2A);
	kfs_keyboard_feed_scancode(0x0F); /* tab */
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('\t', color), (long long)stub[0]);
	kfs_keyboard_feed_scancode(0xAA);
}

KFS_TEST(test_keyboard_alt_function_out_of_range)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x38);
	kfs_keyboard_feed_scancode(0x3F); /* F5 -> target 4 which is >= count */
	kfs_keyboard_feed_scancode(0xBF);
	kfs_keyboard_feed_scancode(0xB8);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
}

KFS_TEST(test_keyboard_extended_e1_sequence)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0xE1);
	kfs_keyboard_feed_scancode(0x1D);
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, (long long)row);
	KFS_ASSERT_EQ(0, (long long)col);
}

KFS_TEST(test_keyboard_backspace_at_origin_noop)
{
	setup_keyboard();
	kfs_keyboard_feed_scancode(0x0E);
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, (long long)row);
	KFS_ASSERT_EQ(0, (long long)col);
}

KFS_TEST(test_keyboard_init_drains_pending_scancodes)
{
	setup_keyboard_common(0);
	kfs_stub_push_keyboard_scancode(0x1E);
	kfs_stub_push_keyboard_scancode(0x9E);
	KFS_ASSERT_EQ(2, (long long)kfs_stub_keyboard_queue_size());
	kfs_keyboard_init();
	KFS_ASSERT_EQ(0, (long long)kfs_stub_keyboard_queue_size());
}

KFS_TEST(test_keyboard_poll_requires_initialization)
{
	setup_keyboard_common(0);
	kfs_stub_push_keyboard_scancode(0x1E);
	kfs_keyboard_poll();
	KFS_ASSERT_EQ(1, (long long)kfs_stub_keyboard_queue_size());
}

KFS_TEST(test_keyboard_poll_consumes_queue)
{
	setup_keyboard();
	kfs_stub_push_keyboard_scancode(0x1E);
	kfs_stub_push_keyboard_scancode(0x9E);
	kfs_keyboard_poll();
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('a', color), (long long)stub[0]);
	KFS_ASSERT_EQ(0, (long long)kfs_stub_keyboard_queue_size());
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_keyboard_poll_requires_initialization),
	KFS_REGISTER_TEST(test_keyboard_init_drains_pending_scancodes),
	KFS_REGISTER_TEST(test_keyboard_letters_shift_caps),
	KFS_REGISTER_TEST(test_keyboard_digits_and_symbols),
	KFS_REGISTER_TEST(test_keyboard_backspace),
	KFS_REGISTER_TEST(test_keyboard_alt_function_switch_console),
	KFS_REGISTER_TEST(test_keyboard_backspace_previous_row),
	KFS_REGISTER_TEST(test_keyboard_extended_sequence_ignored),
	KFS_REGISTER_TEST(test_keyboard_poll_consumes_queue),
	KFS_REGISTER_TEST(test_keyboard_right_shift_uppercase),
	KFS_REGISTER_TEST(test_keyboard_unknown_key_is_ignored),
	KFS_REGISTER_TEST(test_keyboard_shift_nonmapped_falls_back),
	KFS_REGISTER_TEST(test_keyboard_alt_function_out_of_range),
	KFS_REGISTER_TEST(test_keyboard_extended_e1_sequence),
	KFS_REGISTER_TEST(test_keyboard_backspace_at_origin_noop),
};

int register_host_tests_keyboard(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
