#include "../support/terminal_test_support.h"
#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/console.h>

#include <kfs/stdint.h>

/* Provide weak symbol overrides & terminal buffer injection to execute start_kernel */
extern void start_kernel(void);

/* shell_run()をオーバーライド（weak symbolのため） */
void shell_run(void)
{
	/* テスト環境では即座に戻る（無限ループに入らない） */
	return;
}

/* Serial I/O overrides provided by shared stub (serial_io_stub.c) */

static uint16_t term_stub[80 * 25];
static inline uint16_t cell(char c, uint8_t color)
{
	return (uint16_t)c | (uint16_t)color << 8;
}

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

KFS_TEST(test_start_kernel_does_not_crash)
{
	kfs_terminal_set_buffer(term_stub);

	/* shell_run()がすぐに戻るため、start_kernel()も正常に終了する */
	start_kernel();

	/* クラッシュせずにここに到達すればテスト成功 */
	KFS_ASSERT_TRUE(1);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_start_kernel_does_not_crash, setup_test, teardown_test),
};

int register_host_tests_start_kernel(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
