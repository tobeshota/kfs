#include "../host_test_framework.h"
#include "../test_reset.h"
#include <kfs/printk.h>
#include <kfs/string.h>

/* snprintf()のテスト */
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

KFS_TEST(test_snprintf_basic)
{
	char buf[32];
	int ret;

	/* 基本的な整数フォーマット */
	ret = snprintf(buf, sizeof(buf), "Value: %d", 42);
	KFS_ASSERT_EQ(0, strcmp(buf, "Value: 42"));
	KFS_ASSERT_EQ(9, ret); /* "Value: 42" = 9文字 */

	/* 文字列フォーマット */
	ret = snprintf(buf, sizeof(buf), "Hello %s", "World");
	KFS_ASSERT_EQ(0, strcmp(buf, "Hello World"));
	KFS_ASSERT_EQ(11, ret);
}

/* snprintf()でNULL文字列を処理するテスト */
KFS_TEST(test_snprintf_null_string)
{
	char buf[32];
	const char *null_str = NULL;

	/* NULLポインタは"(null)"として出力される */
	snprintf(buf, sizeof(buf), "Pointer: %s", null_str);
	KFS_ASSERT_EQ(0, strcmp(buf, "Pointer: (null)"));
}

/* snprintf()でバッファサイズ制限のテスト */
KFS_TEST(test_snprintf_size_limit)
{
	char buf[10];
	int ret;

	/* バッファサイズを超える文字列 */
	ret = snprintf(buf, sizeof(buf), "This is a very long string");

	/* バッファは9文字+NULL終端で切り詰められる */
	KFS_ASSERT_EQ(9, strlen(buf));
	KFS_ASSERT_EQ('\0', buf[9]);

	/* 戻り値は切り詰められずに書き込まれた場合の長さ */
	KFS_ASSERT_TRUE(ret >= 9);
}

/* printk()でKERN_DEFAULTログレベルのテスト */
KFS_TEST(test_printk_kern_default)
{
	/* KERN_DEFAULT ("<d>") はデフォルトログレベルを設定 */
	printk(KERN_DEFAULT "Default level message\n");

	/* クラッシュしなければOK */
	KFS_ASSERT_TRUE(1);
}

/* printk()でKERN_CONTログレベルのテスト */
KFS_TEST(test_printk_kern_cont)
{
	/* KERN_CONT ("<c>") は継続メッセージを示す */
	printk(KERN_INFO "Starting message...");
	printk(KERN_CONT "continued\n");

	/* クラッシュしなければOK */
	KFS_ASSERT_TRUE(1);
}

/* printk()で複数のログレベルのテスト */
KFS_TEST(test_printk_multiple_levels)
{
	printk(KERN_EMERG "Emergency message\n");
	printk(KERN_ALERT "Alert message\n");
	printk(KERN_CRIT "Critical message\n");
	printk(KERN_ERR "Error message\n");
	printk(KERN_WARNING "Warning message\n");
	printk(KERN_NOTICE "Notice message\n");
	printk(KERN_INFO "Info message\n");
	printk(KERN_DEBUG "Debug message\n");

	/* クラッシュしなければOK */
	KFS_ASSERT_TRUE(1);
}

/* vsnprintf()経由での複雑なフォーマットテスト */
KFS_TEST(test_snprintf_complex_format)
{
	char buf[64];

	/* 16進数フォーマット */
	snprintf(buf, sizeof(buf), "Hex: 0x%x", 255);
	KFS_ASSERT_EQ(0, strcmp(buf, "Hex: 0xff"));

	/* ポインタフォーマット */
	int value = 42;
	snprintf(buf, sizeof(buf), "Ptr: %p", (void *)&value);
	/* ポインタは環境依存なので、フォーマットされたことだけ確認 */
	KFS_ASSERT_TRUE(strlen(buf) > 5);

	/* 文字フォーマット */
	snprintf(buf, sizeof(buf), "Char: %c", 'A');
	KFS_ASSERT_EQ(0, strcmp(buf, "Char: A"));
}

/* snprintf()で空のバッファサイズのテスト */
KFS_TEST(test_snprintf_zero_size)
{
	char buf[1] = {'X'}; /* 初期値を設定 */

	/* サイズ0で呼び出す - バッファは変更されないはず */
	int ret = snprintf(buf, 0, "Hello");

	/* バッファは変更されていないはず */
	KFS_ASSERT_EQ('X', buf[0]);

	/* 戻り値は書き込まれるはずだった長さ */
	KFS_ASSERT_EQ(5, ret);
}

/* printk()でフォーマット指定子のエッジケース */
KFS_TEST(test_printk_format_edge_cases)
{
	/* パーセント記号のエスケープ */
	printk("100%% complete\n");

	/* 負の数 */
	printk("Negative: %d\n", -42);

	/* ゼロ */
	printk("Zero: %d\n", 0);

	/* 複数の引数 */
	printk("Multiple: %d %s %x\n", 123, "test", 0xABC);

	/* クラッシュしなければOK */
	KFS_ASSERT_TRUE(1);
}

/*
 * test_snprintf_pointer - %pフォーマット指定子のテスト
 *
 * 何を検証するか:
 * %pでポインタが0xプレフィックス付きの16進数として正しくフォーマットされること
 *
 * 検証の目的:
 * ポインタ値のデバッグ出力が正しく機能することを確認
 */
KFS_TEST(test_snprintf_pointer)
{
	char buf[64];
	void *ptr = (void *)0x12345678;

	snprintf(buf, sizeof(buf), "ptr=%p", ptr);

	/* 0xプレフィックスが付いていること */
	KFS_ASSERT_EQ('0', buf[4]);
	KFS_ASSERT_EQ('x', buf[5]);

	/* 16進数文字列が含まれていること */
	KFS_ASSERT_TRUE(strlen(buf) > 6);
}

/*
 * test_snprintf_unsigned_long - %luフォーマット指定子のテスト
 *
 * 何を検証するか:
 * %luでunsigned long値が10進数として正しくフォーマットされること
 *
 * 検証の目的:
 * 大きな整数値（サイズ、アドレスなど）の出力が正しく機能することを確認
 */
KFS_TEST(test_snprintf_unsigned_long)
{
	char buf[64];
	unsigned long val = 123456789UL;

	snprintf(buf, sizeof(buf), "size=%lu", val);
	KFS_ASSERT_EQ(0, strcmp(buf, "size=123456789"));

	/* ゼロの場合 */
	snprintf(buf, sizeof(buf), "%lu", 0UL);
	KFS_ASSERT_EQ(0, strcmp(buf, "0"));

	/* 最大値付近 */
	snprintf(buf, sizeof(buf), "%lu", 4294967295UL);
	KFS_ASSERT_EQ(0, strcmp(buf, "4294967295"));
}

/*
 * test_snprintf_long_hex - %lxフォーマット指定子のテスト
 *
 * 何を検証するか:
 * %lxでunsigned long値が小文字の16進数として正しくフォーマットされること
 *
 * 検証の目的:
 * アドレスやビットマスクの16進数表示が正しく機能することを確認
 */
KFS_TEST(test_snprintf_long_hex)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "0x%lx", 0xABCDEF12UL);
	KFS_ASSERT_EQ(0, strcmp(buf, "0xabcdef12"));

	/* 小さい値（先頭ゼロは省略される） */
	snprintf(buf, sizeof(buf), "0x%lx", 0x1AUL);
	KFS_ASSERT_EQ(0, strcmp(buf, "0x1a"));

	/* ゼロ */
	snprintf(buf, sizeof(buf), "0x%lx", 0UL);
	KFS_ASSERT_EQ(0, strcmp(buf, "0x0"));
}

/*
 * test_snprintf_long_hex_upper - %lXフォーマット指定子のテスト
 *
 * 何を検証するか:
 * %lXでunsigned long値が大文字の16進数として正しくフォーマットされること
 *
 * 検証の目的:
 * 大文字16進数表示オプションが正しく機能することを確認
 */
KFS_TEST(test_snprintf_long_hex_upper)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "0x%lX", 0xABCDEF12UL);
	KFS_ASSERT_EQ(0, strcmp(buf, "0xABCDEF12"));

	snprintf(buf, sizeof(buf), "0x%lX", 0x1AUL);
	KFS_ASSERT_EQ(0, strcmp(buf, "0x1A"));
}

/*
 * test_snprintf_signed_long - %ldフォーマット指定子のテスト
 *
 * 何を検証するか:
 * %ldでlong値が符号付き10進数として正しくフォーマットされること
 *
 * 検証の目的:
 * 負の値を含む長整数の出力が正しく機能することを確認
 */
KFS_TEST(test_snprintf_signed_long)
{
	char buf[64];

	/* 正の値 */
	snprintf(buf, sizeof(buf), "%ld", 123456789L);
	KFS_ASSERT_EQ(0, strcmp(buf, "123456789"));

	/* 負の値 */
	snprintf(buf, sizeof(buf), "%ld", -987654321L);
	KFS_ASSERT_EQ(0, strcmp(buf, "-987654321"));

	/* ゼロ */
	snprintf(buf, sizeof(buf), "%ld", 0L);
	KFS_ASSERT_EQ(0, strcmp(buf, "0"));
}

/*
 * test_snprintf_mixed_long_formats - 複数のlong系フォーマット指定子の組み合わせテスト
 *
 * 何を検証するか:
 * 1つのフォーマット文字列に%p、%lu、%lx、%ldが混在しても正しく処理されること
 *
 * 検証の目的:
 * 実際のデバッグ出力で使われる複雑なフォーマット文字列が正しく機能することを確認
 */
KFS_TEST(test_snprintf_mixed_long_formats)
{
	char buf[128];
	void *ptr = (void *)0xDEADBEEF;
	unsigned long size = 4096UL;
	long offset = -256L;
	unsigned long flags = 0x123UL;

	snprintf(buf, sizeof(buf), "ptr=%p size=%lu offset=%ld flags=0x%lx", ptr, size, offset, flags);

	/* 各フィールドが含まれていることを確認 */
	KFS_ASSERT_TRUE(strstr(buf, "ptr=0x") != NULL);
	KFS_ASSERT_TRUE(strstr(buf, "size=4096") != NULL);
	KFS_ASSERT_TRUE(strstr(buf, "offset=-256") != NULL);
	KFS_ASSERT_TRUE(strstr(buf, "flags=0x123") != NULL);
}

/*
 * test_snprintf_null_pointer - NULLポインタの%pテスト
 *
 * 何を検証するか:
 * %pでNULLポインタが0x0として表示されること
 *
 * 検証の目的:
 * NULLポインタのデバッグ出力が安全に処理されることを確認
 */
KFS_TEST(test_snprintf_null_pointer)
{
	char buf[64];
	void *null_ptr = NULL;

	snprintf(buf, sizeof(buf), "ptr=%p", null_ptr);
	KFS_ASSERT_EQ(0, strcmp(buf, "ptr=0x0"));
}

/*
 * test_snprintf_long_modifier_invalid - 不正な%l使用のテスト
 *
 * 何を検証するか:
 * %l単独や%lの後に対応していない文字が来た場合、リテラルとして出力されること
 *
 * 検証の目的:
 * 不正なフォーマット指定子に対するロバスト性を確認
 */
KFS_TEST(test_snprintf_long_modifier_invalid)
{
	char buf[64];

	/* %l単独（後続の文字がない） */
	snprintf(buf, sizeof(buf), "test%l", 123);
	/* %lがリテラルとして出力される */
	KFS_ASSERT_TRUE(strstr(buf, "%l") != NULL);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_null_string, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_size_limit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_printk_kern_default, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_printk_kern_cont, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_printk_multiple_levels, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_complex_format, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_zero_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_printk_format_edge_cases, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_pointer, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_unsigned_long, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_long_hex, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_long_hex_upper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_signed_long, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_mixed_long_formats, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_null_pointer, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_snprintf_long_modifier_invalid, setup_test, teardown_test),
};

int register_host_tests_printk(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
