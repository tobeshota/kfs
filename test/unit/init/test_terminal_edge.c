#include "host_test_framework.h"
/* Updated to production terminal */
#include <linux/terminal.h>

void kfs_terminal_set_buffer(uint16_t *buf);
size_t kfs_terminal_get_row(void);
size_t kfs_terminal_get_col(void);
uint8_t kfs_terminal_get_color(void);
uint16_t *kfs_terminal_get_buffer(void);

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* 1. 行末折り返し: 80 桁埋め => 次の位置 row+1, col=0 */
static uint16_t stub[VGA_WIDTH * VGA_HEIGHT];
static inline uint16_t cell(char c, uint8_t color)
{
	return (uint16_t)c | (uint16_t)color << 8;
}

KFS_TEST(test_terminal_wrap_at_line_end)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	for (int i = 0; i < VGA_WIDTH; i++)
		terminal_putchar('A');
	KFS_ASSERT_EQ(1, (long long)kfs_terminal_get_row());
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_get_col());
	KFS_ASSERT_EQ((long long)cell('A', 7), (long long)stub[79]);
}

/* 2. キャリッジリターン: \r で列先頭に戻り上書きされる */
KFS_TEST(test_terminal_carriage_return)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	terminal_writestring("XYZ");
	terminal_putchar('\r');
	terminal_putchar('Q');
	KFS_ASSERT_EQ((long long)cell('Q', 7), (long long)stub[0]);
	KFS_ASSERT_EQ((long long)cell('Y', 7), (long long)stub[1]);
}

/* 3. 連続スクロール: 全行 +1 行で1回, さらに +VGA_HEIGHT 行で複数回 */
KFS_TEST(test_terminal_multiple_scroll)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	for (int i = 0; i < VGA_HEIGHT; i++)
		terminal_writestring("L\n");
	for (int i = 0; i < VGA_HEIGHT; i++)
		terminal_writestring("M\n");
	KFS_ASSERT_EQ((long long)cell('M', 7), (long long)stub[0]);
}

/* 4. 空文字列書き込み: 位置不変 */
KFS_TEST(test_terminal_write_empty_string)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	terminal_writestring("");
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_get_row());
	KFS_ASSERT_EQ(0, (long long)kfs_terminal_get_col());
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_terminal_wrap_at_line_end),
	KFS_REGISTER_TEST(test_terminal_carriage_return),
	KFS_REGISTER_TEST(test_terminal_multiple_scroll),
	KFS_REGISTER_TEST(test_terminal_write_empty_string),
};

int register_host_tests_terminal_edge(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
