#include "../support/terminal_test_support.h"
#include "host_test_framework.h"
#include <linux/console.h>
#include <linux/printk.h>

#include <stdint.h>

/* Provide weak symbol overrides & terminal buffer injection to execute kernel_main */
extern void kernel_main(void);

/* Serial I/O overrides provided by shared stub (serial_io_stub.c) */

static uint16_t term_stub[80 * 25];
static inline uint16_t cell(char c, uint8_t color)
{
	return (uint16_t)c | (uint16_t)color << 8;
}

KFS_TEST(test_kernel_main_writes_messages)
{
	kfs_terminal_set_buffer(term_stub);
	kernel_main();
	uint8_t green = kfs_vga_make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
	uint8_t normal = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	KFS_ASSERT_EQ((long long)cell('4', green), (long long)term_stub[0]);
	KFS_ASSERT_EQ((long long)cell('2', green), (long long)term_stub[1]);
	KFS_ASSERT_EQ((long long)cell('A', normal), (long long)term_stub[KFS_VGA_WIDTH * 1 + 0]);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_kernel_main_writes_messages),
};

int register_host_tests_kernel_main(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
