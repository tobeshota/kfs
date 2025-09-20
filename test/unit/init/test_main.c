#include "host_test_framework.h"
/* Updated to use production terminal implementation directly via
 * buffer injection + accessors (kfs_terminal_*). */
#include "../support/terminal_test_support.h"
#include <linux/console.h>
#include <stdint.h>

// 画面寸法 (main.c と同期) ※テスト内重複を避けるなら共有ヘッダ化検討
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* Basic helper replicating strlen for local check */
static size_t local_strlen(const char *s)
{
	size_t n = 0;
	while (s[n])
		n++;
	return n;
}

KFS_TEST(test_strlen_basic)
{
	KFS_ASSERT_EQ(0, (long long)local_strlen(""));
	KFS_ASSERT_EQ(1, (long long)local_strlen("a"));
	KFS_ASSERT_EQ(5, (long long)local_strlen("hello"));
}

static uint16_t stub[VGA_WIDTH * VGA_HEIGHT];
static inline uint16_t make_cell(char c, uint8_t color)
{
	return (uint16_t)c | (uint16_t)color << 8;
}

KFS_TEST(test_terminal_initialize_clears_screen)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_get_row());
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_get_col());
	KFS_ASSERT_EQ(7, (long long)kfs_terminal_get_color());
	for (int x = 0; x < 5; x++)
		KFS_ASSERT_EQ((long long)make_cell(' ', 7), (long long)stub[x]);
}

KFS_TEST(test_terminal_basic_write_and_newline)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	terminal_writestring("AB\nC");
	KFS_ASSERT_EQ(1, (long long)kfs_terminal_get_row());
	KFS_ASSERT_EQ(1, (long long)kfs_terminal_get_col());
	KFS_ASSERT_EQ((long long)make_cell('A', 7), (long long)stub[0]);
	KFS_ASSERT_EQ((long long)make_cell('B', 7), (long long)stub[1]);
	KFS_ASSERT_EQ((long long)make_cell('C', 7), (long long)stub[80]);
}

KFS_TEST(test_terminal_scrolling)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	for (int i = 0; i < VGA_HEIGHT; i++)
		terminal_writestring("L\n");
	KFS_ASSERT_EQ(24, (long long)kfs_terminal_get_row());
	KFS_ASSERT_EQ((long long)make_cell('L', 7), (long long)stub[0]);
}

// テスト登録
static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_strlen_basic),
	KFS_REGISTER_TEST(test_terminal_initialize_clears_screen),
	KFS_REGISTER_TEST(test_terminal_basic_write_and_newline),
	KFS_REGISTER_TEST(test_terminal_scrolling),
};

int register_host_tests_init_main(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
