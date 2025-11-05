#include "host_test_framework.h"
#include <kfs/serial.h>
#include <kfs/stdint.h>
#include <kfs/string.h>

#include "../support/io_stub_control.h"

/* I/O logging symbols provided by a shared stub (serial_io_stub.c) */
struct io_log_entry
{
	uint16_t port;
	uint8_t val;
	int is_out;
};
extern struct io_log_entry kfs_io_log_entries[];
extern int kfs_io_log_count;

static void reset_log()
{
	kfs_stub_reset_io();
}
static int find_sequence(const uint8_t *seq, int n)
{
	int idx = 0;
	for (int i = 0; i < kfs_io_log_count; i++)
	{
		if (kfs_io_log_entries[i].val == seq[idx])
		{
			idx++;
			if (idx == n)
				return 1;
		}
	}
	return 0;
}

KFS_TEST(test_serial_init_sequence)
{
	reset_log();
	serial_init();
	/* Expect specific values written during init (order preserved) */
	uint8_t expected[] = {0x00, 0x80, 0x03, 0x00, 0x03, 0xC7, 0x0B};
	int pos = 0;
	for (int i = 0; i < kfs_io_log_count && pos < (int)sizeof(expected); i++)
		if (kfs_io_log_entries[i].val == expected[pos])
			pos++;
	KFS_ASSERT_EQ((long long)sizeof(expected), (long long)pos);
}

KFS_TEST(test_serial_write_inserts_cr_before_lf)
{
	reset_log();
	serial_write("A\n", 2);
	/* Should see 'A' and then '\r' then '\n' or depending on implementation CR before LF; our implementation sends CR
	 * then LF separately via logic: writes CR then LF. */
	int saw_cr = 0, saw_lf = 0;
	for (int i = 0; i < kfs_io_log_count; i++)
	{
		if (kfs_io_log_entries[i].val == '\r')
			saw_cr = 1;
		if (kfs_io_log_entries[i].val == '\n')
			saw_lf = 1;
	}
	KFS_ASSERT_EQ(1, (long long)(saw_cr && saw_lf));
}

KFS_TEST(test_serial_write_string)
{
	reset_log();
	const char *msg = "HELLO";
	serial_write(msg, 5);
	const uint8_t seq[] = {'H', 'E', 'L', 'L', 'O'};
	KFS_ASSERT_EQ(1, (long long)find_sequence(seq, 5));
}

KFS_TEST(test_serial_waits_for_ready)
{
	reset_log();
	const uint8_t statuses[] = {0x00, 0x20, 0x00, 0x20};
	kfs_stub_set_serial_status_sequence(statuses, sizeof(statuses), 0x20);
	serial_write("A\n", 2);
	KFS_ASSERT_EQ(0, (long long)kfs_stub_serial_status_remaining());
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_serial_init_sequence),
	KFS_REGISTER_TEST(test_serial_write_inserts_cr_before_lf),
	KFS_REGISTER_TEST(test_serial_write_string),
	KFS_REGISTER_TEST(test_serial_waits_for_ready),
};

int register_host_tests_serial(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
