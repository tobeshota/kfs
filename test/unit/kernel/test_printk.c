#include "../host_test_framework.h"
#include <kfs/printk.h>
#include <kfs/string.h>

/* snprintf()のテスト */
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

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_snprintf_basic),			  KFS_REGISTER_TEST(test_snprintf_null_string),
	KFS_REGISTER_TEST(test_snprintf_size_limit),	  KFS_REGISTER_TEST(test_printk_kern_default),
	KFS_REGISTER_TEST(test_printk_kern_cont),		  KFS_REGISTER_TEST(test_printk_multiple_levels),
	KFS_REGISTER_TEST(test_snprintf_complex_format),  KFS_REGISTER_TEST(test_snprintf_zero_size),
	KFS_REGISTER_TEST(test_printk_format_edge_cases),
};

int register_host_tests_printk(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
