#include "asm-i386/abs.h"
#include "host_test_framework.h"

KFS_TEST(test_abs_zero)
{
	KFS_ASSERT_EQ(0, kfs_abs(0));
}

KFS_TEST(test_abs_positive)
{
	KFS_ASSERT_EQ(5, kfs_abs(5));
}

KFS_TEST(test_abs_negative)
{
	KFS_ASSERT_EQ(7, kfs_abs(-7));
}

// 登録テーブル
static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_abs_zero),
	KFS_REGISTER_TEST(test_abs_positive),
	KFS_REGISTER_TEST(test_abs_negative),
};

int register_host_tests_abs(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
