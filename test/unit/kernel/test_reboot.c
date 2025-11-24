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

uint16_t port;
uint8_t value;

/* オーバーライド */
void machine_restart_kbd(void)
{
	printk("machine_restart_kbd called\n");
	port = 0xFE;
	value = 0x64;
}

KFS_TEST(test_machine_restart_kbd_calls_io_outb)
{
	/* 関数を呼び出す */
	machine_restart_kbd();

	/* ポートと値が正しいことを確認 */
	KFS_ASSERT_TRUE(port == 0xFE);
	KFS_ASSERT_TRUE(value == 0x64);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_machine_restart_kbd_calls_io_outb, setup_test, teardown_test),
};

int register_host_tests_reboot(struct kfs_test_case **out)
{
	*out = cases;
	return sizeof(cases) / sizeof(cases[0]);
}
