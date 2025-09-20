#include "../support/io_stub_control.h"
#include "../support/terminal_test_support.h"
#include "host_test_framework.h"
#include <kfs/console.h>
#include <kfs/printk.h>

#include <kfs/string.h>
#include <string.h>

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
	kfs_stub_reset_io();
	kfs_printk_set_console_loglevel(kfs_printk_get_default_loglevel());
}

static uint16_t stub[KFS_VGA_WIDTH * KFS_VGA_HEIGHT];

static inline uint16_t make_cell(char c, uint8_t color)
{
	return (uint16_t)c | ((uint16_t)color << 8);
}

static size_t capture_serial(char *dst, size_t max_len)
{
	size_t idx = 0;
	for (int i = 0; i < kfs_io_log_count && idx + 1 < max_len; i++)
	{
		if (kfs_io_log_entries[i].port == 0x3F8)
			dst[idx++] = (char)kfs_io_log_entries[i].val;
	}
	dst[idx] = '\0';
	return idx;
}

KFS_TEST(test_printk_formats_and_output)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	const char *expected = "value=-42 42 0x2a str Z %";
	int len = printk("value=%d %u 0x%x %s %c %%", -42, 42u, 0x2au, "str", 'Z');
	KFS_ASSERT_EQ((long long)strlen(expected), (long long)len);
	char captured[128];
	capture_serial(captured, sizeof(captured));
	for (size_t i = 0; i < strlen(expected); i++)
	{
		KFS_ASSERT_EQ((long long)expected[i], (long long)captured[i]);
		KFS_ASSERT_EQ((long long)make_cell(expected[i], kfs_terminal_get_color()), (long long)stub[i]);
	}
}

KFS_TEST(test_printk_percent_without_specifier)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	int len = printk("%");
	char captured[8];
	size_t n = capture_serial(captured, sizeof(captured));
	KFS_ASSERT_EQ(1, (long long)len);
	KFS_ASSERT_EQ(1, (long long)n);
	KFS_ASSERT_EQ('%', captured[0]);
}

KFS_TEST(test_printk_handles_null_string_and_uppercase)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	int len = printk("%s %X %q", (const char *)0, 0x2Au, 0x2Au);
	char captured[64];
	size_t n = capture_serial(captured, sizeof(captured));
	const char *expected = "(null) 2A %q";
	KFS_ASSERT_EQ((long long)strlen(expected), (long long)n);
	KFS_ASSERT_EQ((long long)len, (long long)n);
	for (size_t i = 0; i < strlen(expected); i++)
		KFS_ASSERT_EQ((long long)expected[i], (long long)captured[i]);
}

KFS_TEST(test_printk_zero_values_and_empty_message)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	printk("");
	KFS_ASSERT_EQ(0, kfs_io_log_count);
	reset_io_log();
	int len = printk("%d %u %x", 0, 0u, 0u);
	char captured[32];
	size_t n = capture_serial(captured, sizeof(captured));
	const char *expected = "0 0 0";
	KFS_ASSERT_EQ((long long)strlen(expected), (long long)n);
	KFS_ASSERT_EQ((long long)len, (long long)n);
	for (size_t i = 0; i < strlen(expected); i++)
		KFS_ASSERT_EQ((long long)expected[i], (long long)captured[i]);
}

KFS_TEST(test_printk_truncates_long_output)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	char payload[600];
	for (int i = 0; i < 599; i++)
		payload[i] = 'A';
	payload[599] = '\0';
	int len = printk("%s", payload);
	char captured[520];
	size_t n = capture_serial(captured, sizeof(captured));
	KFS_ASSERT_TRUE(kfs_io_log_count > 0);
	KFS_ASSERT_TRUE(len > 500);
	KFS_ASSERT_EQ(511, (long long)n);
	KFS_ASSERT_EQ('A', captured[0]);
	KFS_ASSERT_EQ('A', captured[510]);
}

KFS_TEST(test_printk_snprintf_size_zero)
{
	char buf[4] = {'x', 'x', 'x', 'x'};
	int len = kfs_snprintf(buf, 0, "abc");
	KFS_ASSERT_EQ(3, (long long)len);
	KFS_ASSERT_EQ('x', buf[0]);
}

KFS_TEST(test_printk_snprintf_truncates_and_terminates)
{
	char buf[4] = {0};
	int len = kfs_snprintf(buf, sizeof(buf), "hello");
	KFS_ASSERT_EQ(5, (long long)len);
	KFS_ASSERT_EQ('h', buf[0]);
	KFS_ASSERT_EQ('\0', buf[3]);
}

KFS_TEST(test_printk_respects_console_loglevel)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	kfs_printk_set_console_loglevel(KFS_LOGLEVEL_ERR);
	printk(KERN_INFO "hidden");
	KFS_ASSERT_EQ(0, kfs_io_log_count);
	reset_io_log();
	kfs_printk_set_console_loglevel(KFS_LOGLEVEL_INFO);
	printk(KERN_ERR "ERR");
	char captured[16];
	size_t n = capture_serial(captured, sizeof(captured));
	KFS_ASSERT_EQ(3, (long long)n);
	KFS_ASSERT_EQ('E', captured[0]);
}

KFS_TEST(test_printk_continuation_honours_previous_output)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	kfs_printk_set_console_loglevel(KFS_LOGLEVEL_DEBUG);
	printk(KERN_INFO "Hello");
	printk(KERN_CONT " world");
	char captured[32];
	size_t n = capture_serial(captured, sizeof(captured));
	KFS_ASSERT_EQ((long long)strlen("Hello world"), (long long)n);
	KFS_ASSERT_EQ(0, strncmp("Hello world", captured, n));
	reset_io_log();
	kfs_printk_set_console_loglevel(KFS_LOGLEVEL_ERR);
	printk(KERN_DEBUG "nope");
	printk(KERN_CONT " tail");
	KFS_ASSERT_EQ(0, kfs_io_log_count);
}

KFS_TEST(test_printk_default_loglevel_restored)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	kfs_printk_set_console_loglevel(KFS_LOGLEVEL_ERR);
	printk(KERN_DEFAULT "warn");
	KFS_ASSERT_EQ(0, kfs_io_log_count);
	reset_io_log();
	printk(KERN_DEFAULT "warn");
	KFS_ASSERT_TRUE(kfs_io_log_count > 0);
}

KFS_TEST(test_printk_handles_unknown_prefix_codes)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
	reset_io_log();
	printk(KFS_KERN_SOH "/unknown");
	char captured[32];
	size_t n = capture_serial(captured, sizeof(captured));
	KFS_ASSERT_TRUE(n > 1);
	KFS_ASSERT_EQ(1, (long long)captured[0]);
	KFS_ASSERT_EQ('/', captured[1]);
	reset_io_log();
	printk(KFS_KERN_SOH "xtrail");
	n = capture_serial(captured, sizeof(captured));
	KFS_ASSERT_TRUE(n > 1);
	KFS_ASSERT_EQ(1, (long long)captured[0]);
	KFS_ASSERT_EQ('x', captured[1]);
}

KFS_TEST(test_printk_clamps_console_loglevel)
{
	kfs_printk_set_console_loglevel(-5);
	KFS_ASSERT_EQ(minimum_console_loglevel, kfs_printk_get_console_loglevel());
	kfs_printk_set_console_loglevel(42);
	KFS_ASSERT_EQ(KFS_LOGLEVEL_DEBUG, kfs_printk_get_console_loglevel());
	/* restore default */
	kfs_printk_set_console_loglevel(kfs_printk_get_default_loglevel());
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_printk_formats_and_output),
	KFS_REGISTER_TEST(test_printk_percent_without_specifier),
	KFS_REGISTER_TEST(test_printk_handles_null_string_and_uppercase),
	KFS_REGISTER_TEST(test_printk_zero_values_and_empty_message),
	KFS_REGISTER_TEST(test_printk_truncates_long_output),
	KFS_REGISTER_TEST(test_printk_snprintf_size_zero),
	KFS_REGISTER_TEST(test_printk_snprintf_truncates_and_terminates),
	KFS_REGISTER_TEST(test_printk_respects_console_loglevel),
	KFS_REGISTER_TEST(test_printk_continuation_honours_previous_output),
	KFS_REGISTER_TEST(test_printk_default_loglevel_restored),
	KFS_REGISTER_TEST(test_printk_handles_unknown_prefix_codes),
	KFS_REGISTER_TEST(test_printk_clamps_console_loglevel),
};

int register_host_tests_printk(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
