/*
 * test_memory_host.c - test_memory.cのホストテスト登録ラッパー
 */

#include "host_test_framework.h"

/* test_memory.cで定義されたテスト登録関数 */
extern void register_memory_tests(void);

/* テストケース配列（静的） */
static struct kfs_test_case memory_test_cases[64];
static int memory_test_count = 0;

/* グローバルなテストケース追加関数 */
void kfs_register_test_case(const char *name, void (*test_func)(void))
{
	if (memory_test_count < 64)
	{
		memory_test_cases[memory_test_count].name = name;
		memory_test_cases[memory_test_count].fn = test_func;
		memory_test_count++;
	}
}

/* ホストテストシステム用の登録関数 */
int register_host_tests_memory(struct kfs_test_case **out)
{
	if (memory_test_count == 0)
	{
		register_memory_tests();
	}
	*out = memory_test_cases;
	return memory_test_count;
}
