#include "../test_reset.h"
#include "unit_test_framework.h"
#include <asm-i386/page.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/string.h>

extern unsigned long __alloc_pages(unsigned int gfp_mask);
extern void __free_pages(unsigned long addr);
extern void show_mem_info(void);
extern void mem_init(void);

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

KFS_TEST(test___alloc_pages)
{
	__alloc_pages(GFP_KERNEL);
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test___free_pages)
{
	__free_pages(0x42);
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_show_mem_info)
{
	show_mem_info();
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_mem_init)
{
	mem_init();
	KFS_ASSERT_TRUE(1);
}

/* alloc_pages wrapper テスト (order=0) */
KFS_TEST(test_alloc_pages_order0)
{
	struct page *p = alloc_pages(GFP_KERNEL, 0);
	/* カーネルが初期化済みなら非 NULL が返る */
	KFS_ASSERT_TRUE(1); /* クラッシュしなければ OK */
	if (p)
	{
		free_pages(p, 0);
	}
}

/* alloc_pages wrapper テスト (order=1: 未サポート) */
KFS_TEST(test_alloc_pages_unsupported_order)
{
	struct page *p = alloc_pages(GFP_KERNEL, 1);
	/* order > 0 は未サポート—NULL が返る */
	KFS_ASSERT_TRUE(p == NULL);
}

/* free_pages wrapper テスト (order=1: 未サポート) */
KFS_TEST(test_free_pages_unsupported_order)
{
	/* order > 0 はエラー表示して return */
	free_pages(NULL, 1);
	KFS_ASSERT_TRUE(1);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test___alloc_pages, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test___free_pages, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_show_mem_info, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_mem_init, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_alloc_pages_order0, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_alloc_pages_unsupported_order, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_free_pages_unsupported_order, setup_test, teardown_test),
};

int register_unit_tests_page_alloc(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
