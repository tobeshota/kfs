#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/reboot.h>
#include <kfs/stdint.h>

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 必要なら後処理（現在は空） */
}

int called = 0;

/* オーバーライド */
void machine_restart_kbd(void)
{
	called = 1;
}

KFS_TEST(test_machine_restart_kbd_calls_io_outb)
{
	/* 関数を呼び出す */
	machine_restart_kbd();

	KFS_ASSERT_TRUE(called == 1);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_machine_restart_kbd_calls_io_outb, setup_test, teardown_test),
};

int register_host_tests_reboot(struct kfs_test_case **out)
{
	*out = cases;
	return sizeof(cases) / sizeof(cases[0]);
}
