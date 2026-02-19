/**
 * @file test_sys.c
 * @brief sys_getuid / sys_setuid / sys_capget / sys_capset の単体テスト
 */

#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/capability.h>
#include <kfs/errno.h>
#include <kfs/sched.h>
#include <kfs/sys.h>

extern struct task_struct *current;
extern struct task_struct init_task;

static void setup_test(void)
{
	reset_all_state_for_test();
	current = &init_task;
	current->uid.val  = 0;
	current->euid.val = 0;
	current->cap_effective = CAP_FULL_SET;
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
	kernel_cap_t got     = CAP_FULL_SET;

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

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_getuid_returns_uid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setuid_succeeds_with_cap, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_setuid_fails_without_cap, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capget_pid0_returns_current, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capget_invalid_pid_returns_esrch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capset_pid0_updates_current, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_capset_invalid_pid_returns_esrch, setup_test, teardown_test),
};

int register_unit_tests_sys(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
