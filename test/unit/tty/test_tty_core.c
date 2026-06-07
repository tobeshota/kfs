#include "../support/terminal_test_support.h"
#include "../test_reset.h"
#include "unit_test_framework.h"

#include <kfs/console.h>
#include <kfs/tty.h>

static uint16_t stub[KFS_VGA_WIDTH * KFS_VGA_HEIGHT];

static inline char get_char_at(size_t pos)
{
	return (char)(stub[pos] & 0xFF);
}

static void setup_terminal(void)
{
	kfs_terminal_set_buffer(stub);
	terminal_initialize();
}

static void setup_test(void)
{
	reset_all_state_for_test();
	setup_terminal();
	tty_reset();
}

static void teardown_test(void)
{
}

KFS_TEST(test_tty_core_canonical_read_after_enter)
{
	char buf[16];

	tty_input_char_for_console(0, 'a');
	tty_input_char_for_console(0, 'b');
	tty_handle_enter_for_console(0);

	KFS_ASSERT_EQ(2, tty_read_line_for_console(0, buf, sizeof(buf)));
	KFS_ASSERT_EQ('a', buf[0]);
	KFS_ASSERT_EQ('b', buf[1]);
	KFS_ASSERT_EQ('\0', buf[2]);
}

KFS_TEST(test_tty_core_backspace_edits_canonical_line)
{
	char buf[16];

	tty_input_char_for_console(0, 'a');
	tty_input_char_for_console(0, 'b');
	tty_handle_backspace_for_console(0);
	tty_handle_enter_for_console(0);

	KFS_ASSERT_EQ(1, tty_read_line_for_console(0, buf, sizeof(buf)));
	KFS_ASSERT_EQ('a', buf[0]);
	KFS_ASSERT_EQ('\0', buf[1]);
}

KFS_TEST(test_tty_core_echo_on_writes_to_terminal)
{
	tty_input_char_for_console(0, 'x');
	KFS_ASSERT_EQ('x', get_char_at(0));
}

KFS_TEST(test_tty_core_echo_off_suppresses_terminal_write)
{
	KFS_ASSERT_EQ(0, tty_set_echo_for_console(0, 0));
	tty_input_char_for_console(0, 'x');
	KFS_ASSERT_EQ(' ', get_char_at(0));
	KFS_ASSERT_EQ(0, tty_get_echo_for_console(0));
}

int register_unit_tests_tty_core(struct kfs_test_case **out)
{
	static struct kfs_test_case cases[] = {
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_canonical_read_after_enter, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_backspace_edits_canonical_line, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_echo_on_writes_to_terminal, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_echo_off_suppresses_terminal_write, setup_test, teardown_test),
	};

	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
