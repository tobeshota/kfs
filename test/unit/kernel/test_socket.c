#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/pty.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/slab.h>
#include <kfs/socket.h>
#include <kfs/string.h>
#include <kfs/syscall.h>
#include <kfs/wait.h>

extern struct task_struct *current;
extern struct task_struct init_task;
extern void fork_init(void);
extern pid_t kernel_thread(void (*fn)(void), const char *name);
extern void pid_init(void);
extern void init_idle_task(void);
extern void sys_exit(int error_code);
extern int nr_threads;

static void setup_test(void)
{
	reset_all_state_for_test();
	kmem_cache_init();
	pid_init();
	init_idle_task();
	fork_init();
	nr_threads = 0;
	current = &init_task;
	current->pid = 42;
	current->pending.signal = 0;
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

/** endpoint間で双方向に送受信できることを確かめる */
static void test_socketpair_bidirectional_io(void)
{
	int sv[2];
	char buf[8];

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	KFS_ASSERT_EQ(4, (int)unix_socket_write(sv[0], "ping", 4));
	KFS_ASSERT_EQ(4, (int)unix_socket_read(sv[1], buf, sizeof(buf)));
	KFS_ASSERT_EQ(0, memcmp(buf, "ping", 4));
	KFS_ASSERT_EQ(4, (int)unix_socket_write(sv[1], "pong", 4));
	KFS_ASSERT_EQ(4, (int)unix_socket_read(sv[0], buf, sizeof(buf)));
	KFS_ASSERT_EQ(0, memcmp(buf, "pong", 4));
}

/** 部分read後も残りのbyte順序が維持されることを確かめる */
static void test_socketpair_partial_read_preserves_order(void)
{
	int sv[2];
	char buf[8];

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	KFS_ASSERT_EQ(6, (int)unix_socket_write(sv[0], "abcdef", 6));
	KFS_ASSERT_EQ(2, (int)unix_socket_read(sv[1], buf, 2));
	KFS_ASSERT_EQ(0, memcmp(buf, "ab", 2));
	KFS_ASSERT_EQ(4, (int)unix_socket_read(sv[1], buf, sizeof(buf)));
	KFS_ASSERT_EQ(0, memcmp(buf, "cdef", 4));
}

/** peer buffer容量分だけ部分writeすることを確かめる */
static void test_socketpair_write_is_limited_by_buffer(void)
{
	int sv[2];
	char data[UNIX_SOCKET_BUF_SIZE + 1];

	memset(data, 1, sizeof(data));
	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	KFS_ASSERT_EQ(UNIX_SOCKET_BUF_SIZE, (int)unix_socket_write(sv[0], data, sizeof(data)));
	KFS_ASSERT_EQ(0, (int)unix_socket_write(sv[0], data, 1));
}

/** socket fdがsyscall read/write経路へ接続されていることを確かめる */
static void test_socketpair_syscall_read_write(void)
{
	int sv[2];
	char buf[4];

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	KFS_ASSERT_EQ(2, (int)do_syscall(__NR_write, sv[0], (long)"ok", 2, 0, 0));
	KFS_ASSERT_EQ(2, (int)do_syscall(__NR_read, sv[1], (long)buf, sizeof(buf), 0, 0));
	KFS_ASSERT_EQ(0, memcmp(buf, "ok", 2));
}

/** 空bufferのreadが保留signalによって中断されることを確かめる */
static void test_socketpair_read_returns_eintr_for_pending_signal(void)
{
	int sv[2];
	char buf[4];

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	current->pending.signal = 1UL << SIGUSR1;
	KFS_ASSERT_EQ(-EINTR, (int)unix_socket_read(sv[0], buf, sizeof(buf)));
}

static int blocking_writer_fd;

/** blocking readを起床するテスト用writer */
static void socket_blocking_writer(void)
{
	(void)unix_socket_write(blocking_writer_fd, "wake", 4);
	sys_exit(0);
}

/** readerが先にsleepしてもpeer write後に起床することを確かめる */
static void test_socketpair_blocking_read_wakes_after_write(void)
{
	int sv[2];
	char buf[8];
	pid_t writer_pid;

	KFS_ASSERT_EQ(0, unix_socket_pair(AF_UNIX, SOCK_STREAM, 0, sv));
	blocking_writer_fd = sv[1];
	writer_pid = kernel_thread(socket_blocking_writer, "socket_writer");
	KFS_ASSERT_TRUE(writer_pid > 0);
	KFS_ASSERT_EQ(4, (int)unix_socket_read(sv[0], buf, sizeof(buf)));
	KFS_ASSERT_EQ(0, memcmp(buf, "wake", 4));
	KFS_ASSERT_EQ((int)writer_pid, (int)do_wait(NULL, 0));
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_returns_two_fds, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_fd_range_does_not_overlap_pty, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_rejects_invalid_arguments, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_rejects_pair_limit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_syscall_dispatch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_bidirectional_io, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_partial_read_preserves_order, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_write_is_limited_by_buffer, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_syscall_read_write, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_read_returns_eintr_for_pending_signal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_blocking_read_wakes_after_write, setup_test, teardown_test),
};

int register_unit_tests_socket(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
