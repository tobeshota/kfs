#include "../support/terminal_test_support.h"
#include "../test_reset.h"
#include "unit_test_framework.h"

#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/termios.h>
#include <kfs/tty.h>

extern struct task_struct *current;

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
	current->pending.signal = 0;
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

KFS_TEST(test_tty_core_termios_lflag_get_set)
{
	struct termios tio;

	KFS_ASSERT_EQ(0, tty_get_termios_for_console(0, &tio));
	KFS_ASSERT_EQ(ICANON | ECHO | ISIG, (int)tio.c_lflag);

	tio.c_lflag = ISIG;
	KFS_ASSERT_EQ(0, tty_set_termios_for_console(0, &tio));
	KFS_ASSERT_EQ(0, tty_get_echo_for_console(0));
	KFS_ASSERT_EQ(1, tty_signal_enabled_for_console(0));

	KFS_ASSERT_EQ(0, tty_get_termios_for_console(0, &tio));
	KFS_ASSERT_EQ(ISIG, (int)tio.c_lflag);
}

KFS_TEST(test_tty_core_noncanonical_read_without_enter)
{
	struct termios tio;
	char buf[16];

	KFS_ASSERT_EQ(0, tty_get_termios_for_console(0, &tio));
	tio.c_lflag &= ~((tcflag_t)ICANON);
	KFS_ASSERT_EQ(0, tty_set_termios_for_console(0, &tio));

	tty_input_char_for_console(0, 'x');
	KFS_ASSERT_EQ(1, tty_read_line_for_console(0, buf, sizeof(buf)));
	KFS_ASSERT_EQ('x', buf[0]);
	KFS_ASSERT_EQ('\0', buf[1]);
}

/* フォアグラウンドのプロセスは TTY 読み込みで SIGTTIN を受けない */

KFS_TEST(test_tty_core_vintr_sends_sigint)
{
	char buf[4];

	current->pgrp = 10;
	kfs_terminal_set_foreground_pgrp_for_console(0, 10);
	current->pending.signal = 0;

	tty_input_char_for_console(0, 0x03);

	KFS_ASSERT_TRUE(current->pending.signal & (1UL << SIGINT));
	KFS_ASSERT_EQ(0, tty_read_line_for_console(0, buf, sizeof(buf)));
}

KFS_TEST(test_tty_core_vintr_can_be_remapped_with_cc)
{
	struct termios tio;
	char buf[8];

	current->pgrp = 10;
	kfs_terminal_set_foreground_pgrp_for_console(0, 10);
	current->pending.signal = 0;

	KFS_ASSERT_EQ(0, tty_get_termios_for_console(0, &tio));
	tio.c_lflag &= ~((tcflag_t)ECHO);
	tio.c_cc[VINTR] = 0x01;
	KFS_ASSERT_EQ(0, tty_set_termios_for_console(0, &tio));

	tty_input_char_for_console(0, 0x03);
	tty_handle_enter_for_console(0);
	KFS_ASSERT_EQ(1, tty_read_line_for_console(0, buf, sizeof(buf)));
	KFS_ASSERT_EQ(0x03, (int)buf[0]);
	KFS_ASSERT_EQ(0, (int)(current->pending.signal & (1UL << SIGINT)));

	tty_input_char_for_console(0, 0x01);
	KFS_ASSERT_TRUE(current->pending.signal & (1UL << SIGINT));
}

KFS_TEST(test_tty_core_foreground_read_no_sigttin)
{
	char buf[16];

	/* current を fg pgrp に設定 */
	current->pgrp = 10;
	kfs_terminal_set_foreground_pgrp_for_console(0, 10);

	tty_input_char_for_console(0, 'a');
	tty_handle_enter_for_console(0);

	long ret = tty_read_line_for_console(0, buf, sizeof(buf));

	/* フォアグラウンドなので SIGTTIN なし・正常に読める */
	KFS_ASSERT_EQ(0, (int)((current->pending.signal >> SIGTTIN) & 1));
	KFS_ASSERT_EQ(1, (int)ret);
}

/* バックグラウンドのプロセスが TTY を読もうとすると SIGTTIN が届く */
KFS_TEST(test_tty_core_background_read_sends_sigttin)
{
	char buf[16];

	/* current を bg pgrp に、fg は別のグループに設定 */
	current->pgrp = 99;
	kfs_terminal_set_foreground_pgrp_for_console(0, 10);

	tty_input_char_for_console(0, 'a');
	tty_handle_enter_for_console(0);

	long ret = tty_read_line_for_console(0, buf, sizeof(buf));

	/* バックグラウンドなので SIGTTIN が pending になり -EINTR を返す */
	KFS_ASSERT_TRUE(current->pending.signal & (1UL << SIGTTIN));
	KFS_ASSERT_EQ(-EINTR, (int)ret);
}

/* fg pgrp が 0（orphaned）のときは SIGTTIN を送らない */
KFS_TEST(test_tty_core_orphaned_pgrp_no_sigttin)
{
	char buf[16];

	/* fg pgrp = 0: orphaned group */
	current->pgrp = 99;
	kfs_terminal_set_foreground_pgrp_for_console(0, 0);

	tty_input_char_for_console(0, 'a');
	tty_handle_enter_for_console(0);

	long ret = tty_read_line_for_console(0, buf, sizeof(buf));

	/* orphaned pgrp なので SIGTTIN なし・正常に読める */
	KFS_ASSERT_EQ(0, (int)((current->pending.signal >> SIGTTIN) & 1));
	KFS_ASSERT_EQ(1, (int)ret);
}

int register_unit_tests_tty_core(struct kfs_test_case **out)
{
	static struct kfs_test_case cases[] = {
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_canonical_read_after_enter, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_backspace_edits_canonical_line, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_echo_on_writes_to_terminal, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_echo_off_suppresses_terminal_write, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_termios_lflag_get_set, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_noncanonical_read_without_enter, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_vintr_sends_sigint, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_vintr_can_be_remapped_with_cc, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_foreground_read_no_sigttin, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_background_read_sends_sigttin, setup_test, teardown_test),
		KFS_REGISTER_TEST_WITH_SETUP(test_tty_core_orphaned_pgrp_no_sigttin, setup_test, teardown_test),
	};

	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
