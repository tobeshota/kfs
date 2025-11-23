/*
 * test_slab_host.c - test_slab.cのホストテスト登録ラッパー
 */

#include "host_test_framework.h"

/* test_slab.cで定義されたテスト登録関数 */
extern void register_slab_tests(void);

/* テストケース配列（静的） */
static struct kfs_test_case slab_test_cases[64];
static int slab_test_count = 0;

/* グローバルなテストケース追加関数 */
void kfs_register_test_case_slab(const char *name, void (*test_func)(void))
{
	if (slab_test_count < 64)
	{
		slab_test_cases[slab_test_count].name = name;
		slab_test_cases[slab_test_count].fn = test_func;
		slab_test_count++;
	}
}

/* ホストテストシステム用の登録関数 */
int register_host_tests_slab(struct kfs_test_case **out)
{
	if (slab_test_count == 0)
	{
		register_slab_tests();
	}

	/* 注: メモリ管理の初期化はstart_kernel()内で実行される
	 * test_start_kernel_does_not_crashが先に実行されるため、
	 * スラブアロケータは既に初期化済み */

	*out = slab_test_cases;
	return slab_test_count;
}
