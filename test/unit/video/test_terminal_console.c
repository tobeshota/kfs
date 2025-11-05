#include "../support/terminal_test_support.h"
#include "host_test_framework.h"
#include <kfs/console.h>
#include <kfs/printk.h>

#include <kfs/stdint.h>

struct io_log_entry
{
	uint16_t port;
	uint8_t val;
	int is_out;
};
extern struct io_log_entry kfs_io_log_entries[];
extern int kfs_io_log_count;

static void reset_io_log(void)
{
	kfs_io_log_count = 0;
}

static uint16_t stub[KFS_VGA_WIDTH * KFS_VGA_HEIGHT];

static inline uint16_t make_cell(char c, uint8_t color)
{
	return (uint16_t)c | ((uint16_t)color << 8);
}

static void setup_terminal(void)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
}

KFS_TEST(test_vga_color_helpers)
{
	setup_terminal();
	uint8_t color = kfs_vga_make_color(VGA_COLOR_RED, VGA_COLOR_BLUE);
	KFS_ASSERT_EQ((long long)((VGA_COLOR_RED) | (VGA_COLOR_BLUE << 4)), (long long)color);
	uint16_t entry = kfs_vga_make_entry('X', color);
	KFS_ASSERT_EQ((long long)(((uint16_t)'X') | ((uint16_t)color << 8)), (long long)entry);
	kfs_terminal_set_color(color);
	KFS_ASSERT_EQ((long long)color, (long long)kfs_terminal_get_color());
}

KFS_TEST(test_cursor_movement_updates_hw)
{
	setup_terminal();
	int start = kfs_io_log_count;
	kfs_terminal_move_cursor(3, 5);
	KFS_ASSERT_EQ((long long)(start + 4), (long long)kfs_io_log_count);
	uint16_t pos = (uint16_t)(3 * KFS_VGA_WIDTH + 5);
	KFS_ASSERT_EQ(0x3D4, (long long)kfs_io_log_entries[start + 0].port);
	KFS_ASSERT_EQ(0x0F, (long long)kfs_io_log_entries[start + 0].val);
	KFS_ASSERT_EQ(0x3D5, (long long)kfs_io_log_entries[start + 1].port);
	KFS_ASSERT_EQ((long long)(pos & 0xFF), (long long)kfs_io_log_entries[start + 1].val);
	KFS_ASSERT_EQ(0x3D4, (long long)kfs_io_log_entries[start + 2].port);
	KFS_ASSERT_EQ(0x0E, (long long)kfs_io_log_entries[start + 2].val);
	KFS_ASSERT_EQ(0x3D5, (long long)kfs_io_log_entries[start + 3].port);
	KFS_ASSERT_EQ((long long)((pos >> 8) & 0xFF), (long long)kfs_io_log_entries[start + 3].val);
}

KFS_TEST(test_virtual_console_switching_preserves_buffers)
{
	setup_terminal();
	terminal_writestring("A");
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
	kfs_terminal_switch_console(1);
	KFS_ASSERT_EQ(1, (long long)kfs_terminal_active_console());
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell(' ', color), (long long)stub[0]);
	terminal_putchar('Z');
	KFS_ASSERT_EQ((long long)make_cell('Z', color), (long long)stub[0]);
	kfs_terminal_switch_console(0);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
	KFS_ASSERT_EQ((long long)make_cell('A', kfs_terminal_get_color()), (long long)stub[0]);
	kfs_terminal_switch_console(5);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
	KFS_ASSERT_EQ((long long)KFS_VIRTUAL_CONSOLE_COUNT, (long long)kfs_terminal_console_count());
}

KFS_TEST(test_terminal_null_buffer_paths)
{
	setup_terminal();
	kfs_terminal_set_buffer(NULL);
	terminal_putchar('Z');
	kfs_terminal_switch_console(1);
	kfs_terminal_switch_console(0);
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
}

KFS_TEST(test_terminal_move_cursor_clamps)
{
	setup_terminal();
	kfs_terminal_move_cursor(KFS_VGA_HEIGHT + 4, KFS_VGA_WIDTH + 5);
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ((long long)(KFS_VGA_HEIGHT - 1), (long long)row);
	KFS_ASSERT_EQ((long long)(KFS_VGA_WIDTH - 1), (long long)col);
}

KFS_TEST(test_terminal_get_cursor_null_ignored)
{
	setup_terminal();
	kfs_terminal_get_cursor(NULL, NULL);
	/* Expect no crash and cursor unchanged */
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	KFS_ASSERT_EQ(0, (long long)row);
	KFS_ASSERT_EQ(0, (long long)col);
}

KFS_TEST(test_terminal_switch_console_out_of_range)
{
	setup_terminal();
	kfs_terminal_switch_console(KFS_VIRTUAL_CONSOLE_COUNT);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
}

KFS_TEST(test_terminal_switch_same_console_noop)
{
	setup_terminal();
	kfs_terminal_switch_console(0);
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_active_console());
}

KFS_TEST(test_terminal_write_tab_with_shift)
{
	setup_terminal();
	terminal_putchar('\t');
	uint8_t color = kfs_terminal_get_color();
	KFS_ASSERT_EQ((long long)make_cell('\t', color), (long long)stub[0]);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_vga_color_helpers),
	KFS_REGISTER_TEST(test_cursor_movement_updates_hw),
	KFS_REGISTER_TEST(test_virtual_console_switching_preserves_buffers),
	KFS_REGISTER_TEST(test_terminal_null_buffer_paths),
	KFS_REGISTER_TEST(test_terminal_move_cursor_clamps),
	KFS_REGISTER_TEST(test_terminal_get_cursor_null_ignored),
	KFS_REGISTER_TEST(test_terminal_switch_console_out_of_range),
	KFS_REGISTER_TEST(test_terminal_switch_same_console_noop),
	KFS_REGISTER_TEST(test_terminal_write_tab_with_shift),
};

int register_host_tests_terminal_console(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
