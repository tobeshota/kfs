#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/pty.h>
#include <kfs/sched.h>
#include <kfs/socket.h>
#include <kfs/syscall.h>

extern struct task_struct *current;
extern struct task_struct init_task;

static void setup_test(void)
{
	reset_all_state_for_test();
	current = &init_task;
	current->pid = 42;
}

static void teardown_test(void)
{
}

/** 有効な引数で異なる2つのsocket fdを取得できることを確かめる */
static void test_socketpair_returns_two_fds(void)
{
	int sv[2] = {-1, -1};

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	KFS_ASSERT_TRUE(sv[0] != sv[1]);
	KFS_ASSERT_TRUE(unix_socket_is_fd(sv[0]));
	KFS_ASSERT_TRUE(unix_socket_is_fd(sv[1]));
	KFS_ASSERT_EQ(42, (int)unix_socket_owner(sv[0]));
}

/** socket fd範囲がPTY fd範囲と重複しないことを確かめる */
static void test_socketpair_fd_range_does_not_overlap_pty(void)
{
	int sv[2] = {-1, -1};

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	KFS_ASSERT_TRUE(!pty_is_fd(sv[0]));
	KFS_ASSERT_TRUE(!pty_is_fd(sv[1]));
}

/** 未対応引数とNULL出力先が拒否されることを確かめる */
static void test_socketpair_rejects_invalid_arguments(void)
{
	int sv[2] = {-1, -1};

	KFS_ASSERT_EQ(-EINVAL, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, NULL));
	KFS_ASSERT_EQ(-EAFNOSUPPORT, unix_socket_pair(AF_UNIX + 1, SOCK_STREAM, 0, sv));
	KFS_ASSERT_EQ(-EPROTONOSUPPORT, unix_socket_pair(AF_UNIX, SOCK_STREAM + 1, 0, sv));
	KFS_ASSERT_EQ(-EPROTONOSUPPORT, unix_socket_pair(AF_UNIX, SOCK_STREAM, 1, sv));
}

/** 最大pair数を超える割り当てが拒否されることを確かめる */
static void test_socketpair_rejects_pair_limit(void)
{
	int sv[2];
	int i;

	for (i = 0; i < UNIX_SOCKET_MAX_PAIRS; i++)
	{
		KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	}
	KFS_ASSERT_EQ(-EAGAIN, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
}

/** syscall dispatcherからsocketpairを呼び出せることを確かめる */
static void test_socketpair_syscall_dispatch(void)
{
	int sv[2] = {-1, -1};

	KFS_ASSERT_EQ(0, (int)do_syscall(__NR_socketpair, AF_UNIX, SOCK_STREAM, 0, (long)sv, 0));
	KFS_ASSERT_TRUE(unix_socket_is_fd(sv[0]));
	KFS_ASSERT_TRUE(unix_socket_is_fd(sv[1]));
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_returns_two_fds, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_fd_range_does_not_overlap_pty, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_rejects_invalid_arguments, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_rejects_pair_limit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_syscall_dispatch, setup_test, teardown_test),
};

int register_unit_tests_socket(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
