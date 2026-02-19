/**
 * @file test_capability.c
 * @brief Capability 操作マクロの単体テスト
 */

#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/capability.h>
#include <kfs/sched.h>

/* current は kernel/sched/core.c で定義 */
extern struct task_struct *current;
extern struct task_struct init_task;

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
	current = &init_task;
	current->cap_effective = CAP_FULL_SET;
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
}

/* 指定した Capability ビットがセットされることを確かめる */
KFS_TEST(test_cap_raise_sets_bit)
{
	kernel_cap_t cap = CAP_EMPTY_SET;

	cap_raise(cap, CAP_KILL);

	KFS_ASSERT_TRUE(cap_raised(cap, CAP_KILL));
}

/* 指定した Capability ビット以外は変化しないことを確かめる */
KFS_TEST(test_cap_raise_does_not_affect_other_bits)
{
	kernel_cap_t cap = CAP_EMPTY_SET;

	cap_raise(cap, CAP_KILL);

	KFS_ASSERT_TRUE(!(cap_raised(cap, CAP_CHOWN)));
	KFS_ASSERT_TRUE(!(cap_raised(cap, CAP_SETUID)));
}

/* cap[0]→cap[1] の境界（ビット32）を跨ぐ Capability が正しくセットされることを確かめる */
KFS_TEST(test_cap_raise_high_bit)
{
	kernel_cap_t cap = CAP_EMPTY_SET;

	/* ビット32 は (flag) >> 5 == 1 となり cap[1] を操作する */
	cap_raise(cap, 32);

	KFS_ASSERT_TRUE(cap_raised(cap, 32));
	KFS_ASSERT_TRUE(cap.cap[1] == 1u);  /* cap[1] のビット0のみセット */
	KFS_ASSERT_TRUE(cap.cap[0] == 0u);  /* cap[0] は無変化 */
}

/* 指定した Capability ビットがクリアされることを確かめる */
KFS_TEST(test_cap_lower_clears_bit)
{
	kernel_cap_t cap = CAP_FULL_SET;

	cap_lower(cap, CAP_KILL);

	KFS_ASSERT_TRUE(!(cap_raised(cap, CAP_KILL)));
}

/* 指定した Capability ビット以外は変化しないことを確かめる */
KFS_TEST(test_cap_lower_does_not_affect_other_bits)
{
	kernel_cap_t cap = CAP_FULL_SET;

	cap_lower(cap, CAP_KILL);

	KFS_ASSERT_TRUE(cap_raised(cap, CAP_CHOWN));
	KFS_ASSERT_TRUE(cap_raised(cap, CAP_SETUID));
}

/* 空セットではすべての Capability が 0 を返すことを確かめる */
KFS_TEST(test_cap_raised_returns_zero_on_empty)
{
	kernel_cap_t cap = CAP_EMPTY_SET;

	KFS_ASSERT_TRUE(cap_raised(cap, CAP_CHOWN) == 0);
	KFS_ASSERT_TRUE(cap_raised(cap, CAP_KILL) == 0);
	KFS_ASSERT_TRUE(cap_raised(cap, CAP_SYS_ADMIN) == 0);
}

/* フルセットではすべての Capability が非0を返すことを確かめる */
KFS_TEST(test_cap_raised_returns_nonzero_on_full)
{
	kernel_cap_t cap = CAP_FULL_SET;

	KFS_ASSERT_TRUE(cap_raised(cap, CAP_CHOWN) != 0);
	KFS_ASSERT_TRUE(cap_raised(cap, CAP_KILL) != 0);
	KFS_ASSERT_TRUE(cap_raised(cap, CAP_SYS_ADMIN) != 0);
}

/* cap_effective がフルセットのとき capable() が真を返すことを確かめる */
KFS_TEST(test_capable_returns_true_when_cap_set)
{
	current->cap_effective = CAP_FULL_SET;

	KFS_ASSERT_TRUE(capable(CAP_KILL));
	KFS_ASSERT_TRUE(capable(CAP_SETUID));
	KFS_ASSERT_TRUE(capable(CAP_SYS_ADMIN));
}

/* cap_effective が空セットのとき capable() が偽を返すことを確かめる */
KFS_TEST(test_capable_returns_false_when_cap_empty)
{
	current->cap_effective = CAP_EMPTY_SET;

	KFS_ASSERT_TRUE(!(capable(CAP_KILL)));
	KFS_ASSERT_TRUE(!(capable(CAP_SETUID)));
	KFS_ASSERT_TRUE(!(capable(CAP_SYS_ADMIN)));
}

/* セットした Capability のみ capable() が真を返し，他は偽を返すことを確かめる */
KFS_TEST(test_capable_returns_true_for_specific_cap_only)
{
	current->cap_effective = CAP_EMPTY_SET;
	cap_raise(current->cap_effective, CAP_KILL);

	KFS_ASSERT_TRUE(capable(CAP_KILL));
	KFS_ASSERT_TRUE(!(capable(CAP_SETUID)));
	KFS_ASSERT_TRUE(!(capable(CAP_SYS_ADMIN)));
}

/* cap_lower で下げた Capability は capable() が偽を返すことを確かめる */
KFS_TEST(test_capable_after_cap_lower)
{
	current->cap_effective = CAP_FULL_SET;
	cap_lower(current->cap_effective, CAP_SETUID);

	KFS_ASSERT_TRUE(capable(CAP_KILL));
	KFS_ASSERT_TRUE(!(capable(CAP_SETUID)));
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_raise_sets_bit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_raise_does_not_affect_other_bits, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_raise_high_bit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_lower_clears_bit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_lower_does_not_affect_other_bits, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_raised_returns_zero_on_empty, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cap_raised_returns_nonzero_on_full, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_capable_returns_true_when_cap_set, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_capable_returns_false_when_cap_empty, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_capable_returns_true_for_specific_cap_only, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_capable_after_cap_lower, setup_test, teardown_test),
};

int register_unit_tests_capability(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
