#include "../support/terminal_test_support.h"
#include "host_test_framework.h"
#include <linux/terminal.h>
#include <stdint.h>
#include <string.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Production terminal code reused via injected buffer (support header). */

static uint16_t stub[VGA_WIDTH * VGA_HEIGHT];
static inline uint16_t make_cell(char c, uint8_t color)
{
	return (uint16_t)c | (uint16_t)color << 8;
}

static void clear_stub(uint8_t color)
{
	for (int y = 0; y < VGA_HEIGHT; y++)
		for (int x = 0; x < VGA_WIDTH; x++)
			stub[y * VGA_WIDTH + x] = make_cell(' ', color);
}

static void setup(void)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize(); /* uses injected buffer */
}

KFS_TEST(test_terminal_initialize_clears_screen_prod)
{
	setup();
	for (int x = 0; x < 5; x++)
		KFS_ASSERT_EQ((long long)make_cell(' ', 7), (long long)stub[x]);
}

KFS_TEST(test_terminal_basic_write_and_newline_prod)
{
	setup();
	terminal_writestring("AB\nC");
	KFS_ASSERT_EQ((long long)make_cell('A', 7), (long long)stub[0]);
	KFS_ASSERT_EQ((long long)make_cell('B', 7), (long long)stub[1]);
	KFS_ASSERT_EQ((long long)make_cell('C', 7), (long long)stub[80]);
}

KFS_TEST(test_terminal_wrap_and_scroll_prod)
{
	setup();
	/* Fill exactly 25 lines (0..24) each ending with newline to cause one scroll when writing line 25 */
	for (int i = 0; i < 25; i++)
	{
		for (int x = 0; x < VGA_WIDTH; x++)
			terminal_putchar('A');
		terminal_putchar('\n');
	}
	/* After writing 25 lines, terminal_row should be 24 (last line) due to scroll adjustment. Top-left becomes line 1
	 * of original (all 'A'). */
	KFS_ASSERT_EQ(24, (long long)kfs_terminal_get_row());
	KFS_ASSERT_EQ((long long)make_cell('A', 7), (long long)stub[0]);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_terminal_initialize_clears_screen_prod),
	KFS_REGISTER_TEST(test_terminal_basic_write_and_newline_prod),
	KFS_REGISTER_TEST(test_terminal_wrap_and_scroll_prod),
};

int register_host_tests_terminal_prod(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
