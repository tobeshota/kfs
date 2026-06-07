/**
 * @file test_sys.c
 * @brief sys_getuid / sys_setuid / sys_capget / sys_capset の単体テスト
 */

#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/capability.h>
#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/sched.h>
#include <kfs/sys.h>

extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;

static void setup_test(void)
{
	reset_all_state_for_test();
	current = &init_task;
	current->tty_console = 0;
	current->uid.val = 0;
	current->euid.val = 0;
	current->cap_effective = CAP_FULL_SET;
	(void)kfs_terminal_set_foreground_pgrp_for_console(0, 0);
}

static void teardown_test(void)
{
}

/* sys_getuid が current->uid.val を返すことを確かめる */
KFS_TEST(test_sys_getuid_returns_uid)
{
	current->uid.val = 42;

	KFS_ASSERT_EQ(42, sys_getuid());
}

/* CAP_SETUID を持つとき sys_setuid が uid / euid を更新することを確かめる */
KFS_TEST(test_sys_setuid_succeeds_with_cap)
{
	cap_raise(current->cap_effective, CAP_SETUID);

	KFS_ASSERT_EQ(0, sys_setuid(1000));
	KFS_ASSERT_EQ(1000, (int)current->uid.val);
	KFS_ASSERT_EQ(1000, (int)current->euid.val);
}

/* CAP_SETUID を持たないとき sys_setuid が -EPERM を返すことを確かめる */
KFS_TEST(test_sys_setuid_fails_without_cap)
{
	current->cap_effective = CAP_EMPTY_SET;

	KFS_ASSERT_EQ(-EPERM, sys_setuid(1000));
	KFS_ASSERT_EQ(0, (int)current->uid.val); /* 変化しないこと */
}

/* pid=0 のとき sys_capget が current の cap_effective を返すことを確かめる */
KFS_TEST(test_sys_capget_pid0_returns_current)
{
	kernel_cap_t eff = CAP_FULL_SET;
	kernel_cap_t expected = CAP_EMPTY_SET;

	/* setup_test() の CAP_FULL_SET とは異なる値をセットして
	   current から読んでいることを確かめる */
	cap_raise(expected, CAP_KILL);
	current->cap_effective = expected;

	// pid=0 で current の cap_effective を eff にコピーする．成功して 0 が返るはず．
	KFS_ASSERT_EQ(0, sys_capget(0, &eff, NULL, NULL));

	// eff が expected と同じ値であることを確かめる．
	KFS_ASSERT_TRUE(eff.cap[0] == expected.cap[0]);
	KFS_ASSERT_TRUE(eff.cap[1] == expected.cap[1]);
}

/* 存在しない PID を指定すると sys_capget が -ESRCH を返すことを確かめる */
KFS_TEST(test_sys_capget_invalid_pid_returns_esrch)
{
	kernel_cap_t eff = CAP_EMPTY_SET;

	KFS_ASSERT_EQ(-ESRCH, sys_capget(-42, &eff, NULL, NULL));
}

/* pid=0 のとき sys_capset が current の cap_effective を更新することを確かめる */
KFS_TEST(test_sys_capset_pid0_updates_current)
{
	kernel_cap_t new_cap = CAP_EMPTY_SET;
	kernel_cap_t got = CAP_FULL_SET;

	cap_raise(current->cap_effective, CAP_SETPCAP);
	cap_raise(new_cap, CAP_KILL);

	// pid=0 で current の cap_effective を new_cap に更新する．成功して 0 が返るはず．
	KFS_ASSERT_EQ(0, sys_capset(0, &new_cap, NULL, NULL));
	// pid=0 で current の cap_effective を got にコピーする．成功して 0 が返るはず．
	KFS_ASSERT_EQ(0, sys_capget(0, &got, NULL, NULL));

	// current の cap_effective が new_cap と同じ値になっていることを確かめる．
	KFS_ASSERT_TRUE(got.cap[0] == new_cap.cap[0]);
	KFS_ASSERT_TRUE(got.cap[1] == new_cap.cap[1]);
}

/* 存在しない PID を指定すると sys_capset が -ESRCH を返すことを確かめる */
KFS_TEST(test_sys_capset_invalid_pid_returns_esrch)
{
	kernel_cap_t new_cap = CAP_FULL_SET;

	KFS_ASSERT_EQ(-ESRCH, sys_capset(-42, &new_cap, NULL, NULL));
}

/* sys_setpgid(0, 0) が current を自身の pgrp leader にすることを確かめる */
KFS_TEST(test_sys_setpgid_pid0_updates_current_pgrp)
{
	current->pid = 42;
	current->pgrp = 1;
	current->session = 1;

	KFS_ASSERT_EQ(0, sys_setpgid(0, 0));
	KFS_ASSERT_EQ(42, current->pgrp);
}

/* sys_setpgid で pid != current->pid を指定すると -ESRCH を返すことを確かめる */
KFS_TEST(test_sys_setpgid_other_pid_returns_esrch)
{
	current->pid = 42;
	current->pgrp = 1;

	KFS_ASSERT_EQ(-ESRCH, sys_setpgid(43, 43));
	KFS_ASSERT_EQ(1, current->pgrp); /* 変更されないこと */
}

/* sys_setpgid で不正な pgid を指定すると -EINVAL を返すことを確かめる */
KFS_TEST(test_sys_setpgid_invalid_pgid_returns_einval)
{
	current->pid = 42;
	current->pgrp = 1;

	KFS_ASSERT_EQ(-EINVAL, sys_setpgid(0, -1));
	KFS_ASSERT_EQ(1, current->pgrp); /* 変更されないこと */
}

/* sys_getpgid(0) が current の pgrp を返すことを確かめる */
KFS_TEST(test_sys_getpgid_pid0_returns_current_pgrp)
{
	current->pgrp = 77;

	KFS_ASSERT_EQ(77, sys_getpgid(0));
}

/* 存在しない PID を指定すると sys_getpgid が -ESRCH を返すことを確かめる */
KFS_TEST(test_sys_getpgid_invalid_pid_returns_esrch)
{
	KFS_ASSERT_EQ(-ESRCH, sys_getpgid(-42));
}

/* sys_getpgrp が current の pgrp を返すことを確かめる */
KFS_TEST(test_sys_getpgrp_returns_current_pgrp)
{
	current->pgrp = 123;

	KFS_ASSERT_EQ(123, sys_getpgrp());
}

/* sys_setsid が新しい session / pgrp を作ることを確かめる */
KFS_TEST(test_sys_setsid_creates_new_session_and_pgrp)
{
	current->pid = 42;
	current->pgrp = 7;
	current->session = 7;

	KFS_ASSERT_EQ(42, sys_setsid());
	KFS_ASSERT_EQ(42, current->session);
	KFS_ASSERT_EQ(42, current->pgrp);
}

/* process group leader は sys_setsid できないことを確かめる */
KFS_TEST(test_sys_setsid_rejects_process_group_leader)
{
	current->pid = 42;
	current->pgrp = 42;
	current->session = 7;

	KFS_ASSERT_EQ(-EPERM, sys_setsid());
	KFS_ASSERT_EQ(7, current->session);
	KFS_ASSERT_EQ(42, current->pgrp);
}

/* 別 session のタスクに対する sys_setpgid を拒否することを確かめる */
KFS_TEST(test_sys_setpgid_rejects_target_in_other_session)
{
	struct task_struct child = init_task;

	child.pid = 43;
	child.parent = current;
	child.session = 99;
	child.pgrp = 99;
	INIT_LIST_HEAD(&child.children);
	INIT_LIST_HEAD(&child.sibling);
	INIT_LIST_HEAD(&child.run_list);
	list_add_tail(&child.tasks, &task_list);

	current->pid = 42;
	current->session = 42;
	current->pgrp = 42;

	KFS_ASSERT_EQ(-EPERM, sys_setpgid(43, 43));
	KFS_ASSERT_EQ(99, child.pgrp);
	list_del(&child.tasks);
}

/* sys_tcgetpgrp が foreground_pgrp を返すことを確かめる */
KFS_TEST(test_sys_tcgetpgrp_returns_foreground_pgrp)
{
	(void)kfs_terminal_set_foreground_pgrp_for_console(0, 77);

	KFS_ASSERT_EQ(77, sys_tcgetpgrp(0));
}

/* sys_tcsetpgrp が foreground_pgrp を更新することを確かめる */
KFS_TEST(test_sys_tcsetpgrp_updates_foreground_pgrp)
{
	(void)kfs_terminal_set_foreground_pgrp_for_console(0, 12);

	KFS_ASSERT_EQ(0, sys_tcsetpgrp(123, 99));
	KFS_ASSERT_EQ(99, kfs_terminal_get_foreground_pgrp_for_console(0));
}

/* sys_tcsetpgrp が不正な pgrp を拒否することを確かめる */
KFS_TEST(test_sys_tcsetpgrp_invalid_pgrp_returns_einval)
{
	(void)kfs_terminal_set_foreground_pgrp_for_console(0, 12);

	KFS_ASSERT_EQ(-EINVAL, sys_tcsetpgrp(0, 0));
	KFS_ASSERT_EQ(12, kfs_terminal_get_foreground_pgrp_for_console(0));
}

/** sys_msleep(0) は schedule_timeout を呼ばずに即座に 0 を返すはず
 * 検証対象: kernel/sys.c sys_msleep()
 * 検証項目: ms == 0 のとき早期リターンで 0 が返る
 */
static void test_sys_msleep_zero_returns_immediately(void)
{
	long ret = sys_msleep(0);

	KFS_ASSERT_EQ(0, (int)ret);
	printk("test_sys_msleep_zero_returns_immediately: OK\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_getuid_returns_uid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setuid_succeeds_with_cap, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setuid_fails_without_cap, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capget_pid0_returns_current, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capget_invalid_pid_returns_esrch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capset_pid0_updates_current, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capset_invalid_pid_returns_esrch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setpgid_pid0_updates_current_pgrp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setpgid_other_pid_returns_esrch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setpgid_invalid_pgid_returns_einval, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_getpgid_pid0_returns_current_pgrp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_getpgid_invalid_pid_returns_esrch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_getpgrp_returns_current_pgrp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setsid_creates_new_session_and_pgrp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setsid_rejects_process_group_leader, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setpgid_rejects_target_in_other_session, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_tcgetpgrp_returns_foreground_pgrp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_tcsetpgrp_updates_foreground_pgrp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_tcsetpgrp_invalid_pgrp_returns_einval, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_msleep_zero_returns_immediately, setup_test, teardown_test),
};

int register_unit_tests_sys(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
