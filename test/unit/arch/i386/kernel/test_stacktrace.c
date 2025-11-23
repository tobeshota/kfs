#include "../../../test_reset.h"
#include "host_test_framework.h"
#include <kfs/printk.h>
#include <kfs/stdint.h>

/* stacktrace.cの関数 */
void show_stack(unsigned long *esp);
void dump_stack(void);

/* boot.Sで定義されたスタック領域の境界（テスト環境でも存在） */
extern char stack_bottom[];
extern char stack_top[];

/* 前方宣言 */
size_t test_stacktrace_count(void);

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

/* 基本的なshow_stack呼び出しテスト */
static void test_show_stack_with_null(void)
{
	/* NULLを渡すと現在のスタックポインタを使用する */
	show_stack(NULL);
	/* クラッシュしなければ成功 */
	KFS_ASSERT_EQ(1, 1);
}

/* 明示的にスタックポインタを渡すテスト */
static void test_show_stack_with_explicit_esp(void)
{
	unsigned long dummy_stack[8] = {0x12345678, 0xabcdef00, 0x11111111, 0x22222222,
									0x33333333, 0x44444444, 0x55555555, 0x66666666};
	show_stack(dummy_stack);
	/* クラッシュしなければ成功 */
	KFS_ASSERT_EQ(1, 1);
}

/* dump_stack呼び出しテスト */
static void test_dump_stack(void)
{
	dump_stack();
	/* クラッシュしなければ成功 */
	KFS_ASSERT_EQ(1, 1);
}

/* スタック境界外のポインタを渡すテスト */
static void test_show_stack_out_of_bounds(void)
{
	unsigned long *invalid_ptr = (unsigned long *)0x12345678;
	show_stack(invalid_ptr);
	/* クラッシュしなければ成功（境界チェックで安全に停止） */
	KFS_ASSERT_EQ(1, 1);
}

/* スタック境界ギリギリのテスト */
static void test_show_stack_at_stack_top(void)
{
	/* stack_topの直前のアドレスを使用 */
	unsigned long *near_top = (unsigned long *)((char *)stack_top - sizeof(unsigned long) * 4);
	show_stack(near_top);
	KFS_ASSERT_EQ(1, 1);
}

/* 連続してdump_stackを呼び出すテスト */
static void test_dump_stack_multiple_times(void)
{
	dump_stack();
	dump_stack();
	dump_stack();
	KFS_ASSERT_EQ(1, 1);
}

/* フレームウォークのテスト - 深い呼び出しスタック */
static void test_show_stack_deep_call(void)
{
	/* 現在のスタックから呼び出すので、フレームウォークが動作する */
	show_stack(NULL);
	KFS_ASSERT_EQ(1, 1);
}

struct kfs_test_case test_stacktrace_cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_show_stack_with_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_show_stack_with_explicit_esp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_dump_stack, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_show_stack_out_of_bounds, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_show_stack_at_stack_top, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_dump_stack_multiple_times, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_show_stack_deep_call, setup_test, teardown_test),
};

int register_host_tests_stacktrace(struct kfs_test_case **out)
{
	*out = test_stacktrace_cases;
	return (int)test_stacktrace_count();
}

size_t test_stacktrace_count(void)
{
	return sizeof(test_stacktrace_cases) / sizeof(test_stacktrace_cases[0]);
}
