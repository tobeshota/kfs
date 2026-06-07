#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/list.h>
#include <kfs/pty.h>
#include <kfs/sched.h>
#include <kfs/string.h>
#include <kfs/sys.h>

extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;

static void setup_test(void)
{
	reset_all_state_for_test();
	current = &init_task;
	current->pid = 42;
	current->session = 42;
	current->pgrp = 42;
	pty_reset();
}

static void teardown_test(void)
{
}

KFS_TEST(test_pty_open_returns_master_and_slave_fds)
{
	int master_fd = -1;
	int slave_fd = -1;

	KFS_ASSERT_EQ(0, pty_open(&master_fd, &slave_fd));
	KFS_ASSERT_TRUE(pty_is_master_fd(master_fd));
	KFS_ASSERT_TRUE(pty_is_slave_fd(slave_fd));
}

KFS_TEST(test_pty_master_write_then_slave_read)
{
	int master_fd = -1;
	int slave_fd = -1;
	char out[] = "ping";
	char in[8];
	long nread;

	in[0] = '\0';
	KFS_ASSERT_EQ(0, pty_open(&master_fd, &slave_fd));
	KFS_ASSERT_EQ(4, pty_write(master_fd, out, 4));
	nread = pty_read(slave_fd, in, sizeof(in));
	KFS_ASSERT_EQ(4, (int)nread);
	KFS_ASSERT_EQ(0, memcmp(in, out, 4));
}

KFS_TEST(test_pty_slave_write_then_master_read)
{
	int master_fd = -1;
	int slave_fd = -1;
	char out[] = "pong";
	char in[8];
	long nread;

	in[0] = '\0';
	KFS_ASSERT_EQ(0, pty_open(&master_fd, &slave_fd));
	KFS_ASSERT_EQ(4, pty_write(slave_fd, out, 4));
	nread = pty_read(master_fd, in, sizeof(in));
	KFS_ASSERT_EQ(4, (int)nread);
	KFS_ASSERT_EQ(0, memcmp(in, out, 4));
}

KFS_TEST(test_sys_openpty_returns_valid_fds)
{
	int master_fd = -1;
	int slave_fd = -1;

	KFS_ASSERT_EQ(0, (int)sys_openpty(&master_fd, &slave_fd));
	KFS_ASSERT_TRUE(pty_is_master_fd(master_fd));
	KFS_ASSERT_TRUE(pty_is_slave_fd(slave_fd));
}

KFS_TEST(test_sys_tcsetpgrp_and_tcgetpgrp_on_pty_slave)
{
	struct task_struct child = init_task;
	int master_fd = -1;
	int slave_fd = -1;

	child.pid = 99;
	child.parent = current;
	child.session = current->session;
	child.pgrp = 99;
	INIT_LIST_HEAD(&child.children);
	INIT_LIST_HEAD(&child.sibling);
	INIT_LIST_HEAD(&child.run_list);
	list_add_tail(&child.tasks, &task_list);

	KFS_ASSERT_EQ(0, (int)sys_openpty(&master_fd, &slave_fd));
	KFS_ASSERT_EQ(0, sys_tcsetpgrp(slave_fd, 99));
	KFS_ASSERT_EQ(99, (int)sys_tcgetpgrp(slave_fd));

	list_del(&child.tasks);
}

KFS_TEST(test_sys_tcgetpgrp_rejects_pty_master_fd)
{
	int master_fd = -1;
	int slave_fd = -1;

	KFS_ASSERT_EQ(0, (int)sys_openpty(&master_fd, &slave_fd));
	(void)slave_fd;
	KFS_ASSERT_EQ(-ENOTTY, (int)sys_tcgetpgrp(master_fd));
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_pty_open_returns_master_and_slave_fds, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pty_master_write_then_slave_read, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pty_slave_write_then_master_read, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_openpty_returns_valid_fds, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_tcsetpgrp_and_tcgetpgrp_on_pty_slave, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_tcgetpgrp_rejects_pty_master_fd, setup_test, teardown_test),
};

int register_unit_tests_pty(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
