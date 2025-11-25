/*
 * test_vmalloc.c - Phase 7 仮想メモリアロケータのテスト
 *
 * mm/vmalloc.c の以下の関数をテスト:
 * - vmalloc_init(): 仮想メモリアロケータの初期化
 * - vmalloc(): 仮想メモリ割り当て
 * - vfree(): 仮想メモリ解放
 * - vsize(): 割り当てサイズ取得
 * - vbrk(): ヒープブレイクポイント変更（スタブ）
 */

#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/stddef.h>
#include <kfs/vmalloc.h>

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
	/* vmalloc_init()は既にreset_all_state_for_test()内で呼ばれる */
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 必要なら後処理（現在は空） */
}

/*
 * テスト: vmalloc_init - 初期化
 * 検証: vmalloc_init()が正常に実行されること
 * 目的: 初期化処理の基本動作を確認
 */
KFS_TEST(test_vmalloc_init)
{
	/* vmalloc_init()を呼び出し（既にsetupで呼ばれているが再度確認） */
	vmalloc_init();

	/* 初期化後はvmallocが使用可能であることを確認 */
	void *ptr = vmalloc(4096);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* 後始末 */
	vfree(ptr);
}

/*
 * テスト: vmalloc - サイズ0の割り当て
 * 検証: サイズ0の割り当てはNULLを返すこと
 * 目的: 不正な入力のハンドリングを確認
 */
KFS_TEST(test_vmalloc_zero_size)
{
	void *ptr;

	/* サイズ0で割り当て */
	ptr = vmalloc(0);

	/* NULLが返ること */
	KFS_ASSERT_TRUE(ptr == NULL);
}

/*
 * テスト: vmalloc - 1ページの割り当て
 * 検証: 4096バイト（1ページ）の割り当てが成功すること
 * 目的: 最小単位での基本動作を確認
 */
KFS_TEST(test_vmalloc_one_page)
{
	void *ptr;

	/* 1ページ（4096バイト）を割り当て */
	ptr = vmalloc(4096);

	/* 成功すること */
	KFS_ASSERT_TRUE(ptr != NULL);

	/* 後始末 */
	vfree(ptr);
}

/*
 * テスト: vmalloc - 複数ページの割り当て
 * 検証: 8192バイト（2ページ）の割り当てが成功すること
 * 目的: 複数ページの割り当てを確認
 */
KFS_TEST(test_vmalloc_multiple_pages)
{
	void *ptr;

	/* 2ページ（8192バイト）を割り当て */
	ptr = vmalloc(8192);

	/* 成功すること */
	KFS_ASSERT_TRUE(ptr != NULL);

	/* 後始末 */
	vfree(ptr);
}

/*
 * テスト: vmalloc - ページ境界に揃っていないサイズ
 * 検証: 4100バイトの割り当てが成功し、8192バイトに切り上げられること
 * 目的: ページ境界への切り上げ処理を確認
 */
KFS_TEST(test_vmalloc_unaligned_size)
{
	void *ptr;
	size_t size;

	/* ページ境界に揃っていないサイズで割り当て */
	ptr = vmalloc(4100);

	/* 成功すること */
	KFS_ASSERT_TRUE(ptr != NULL);

	/* サイズは8192バイト（2ページ）に切り上げられること */
	size = vsize(ptr);
	KFS_ASSERT_EQ(8192, size);

	/* 後始末 */
	vfree(ptr);
}

/*
 * テスト: vmalloc - 複数の割り当て
 * 検証: 複数の割り当てが成功すること
 * 目的: メモリリストの管理を確認
 */
KFS_TEST(test_vmalloc_multiple_allocations)
{
	void *ptr1, *ptr2, *ptr3;

	/* 3つの領域を割り当て */
	ptr1 = vmalloc(4096);
	ptr2 = vmalloc(8192);
	ptr3 = vmalloc(16384);

	/* すべて成功すること */
	KFS_ASSERT_TRUE(ptr1 != NULL);
	KFS_ASSERT_TRUE(ptr2 != NULL);
	KFS_ASSERT_TRUE(ptr3 != NULL);

	/* すべて異なるアドレスであること */
	KFS_ASSERT_TRUE(ptr1 != ptr2);
	KFS_ASSERT_TRUE(ptr2 != ptr3);
	KFS_ASSERT_TRUE(ptr1 != ptr3);

	/* 後始末 */
	vfree(ptr1);
	vfree(ptr2);
	vfree(ptr3);
}

/*
 * テスト: vfree - NULL解放
 * 検証: NULLポインタの解放は何もせずに終了すること
 * 目的: 不正な入力のハンドリングを確認
 */
KFS_TEST(test_vfree_null)
{
	/* NULLを解放（クラッシュしないこと） */
	vfree(NULL);

	/* テスト成功 */
	KFS_ASSERT_TRUE(1);
}

/*
 * テスト: vfree - 通常の解放
 * 検証: 割り当てたメモリを正常に解放できること
 * 目的: 基本的な解放処理を確認
 */
KFS_TEST(test_vfree_normal)
{
	void *ptr;

	/* 割り当て */
	ptr = vmalloc(4096);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* 解放 */
	vfree(ptr);

	/* 解放後は再度割り当て可能であること */
	ptr = vmalloc(4096);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* 後始末 */
	vfree(ptr);
}

/*
 * テスト: vfree - 不正なアドレス
 * 検証: 不正なアドレスの解放は警告を出すが、クラッシュしないこと
 * 目的: エラーハンドリングを確認
 */
KFS_TEST(test_vfree_invalid_address)
{
	void *invalid_ptr = (void *)0xDEADBEEF;

	/* 不正なアドレスを解放（警告は出るがクラッシュしない） */
	vfree(invalid_ptr);

	/* テスト成功 */
	KFS_ASSERT_TRUE(1);
}

/*
 * テスト: vsize - NULLアドレス
 * 検証: NULLアドレスのサイズは0を返すこと
 * 目的: 不正な入力のハンドリングを確認
 */
KFS_TEST(test_vsize_null)
{
	size_t size;

	/* NULLのサイズを取得 */
	size = vsize(NULL);

	/* 0が返ること */
	KFS_ASSERT_EQ(0, size);
}

/*
 * テスト: vsize - 正常なアドレス
 * 検証: 割り当てたメモリのサイズを正しく取得できること
 * 目的: サイズ取得の基本動作を確認
 */
KFS_TEST(test_vsize_normal)
{
	void *ptr;
	size_t size;

	/* 4096バイトを割り当て */
	ptr = vmalloc(4096);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* サイズを取得 */
	size = vsize(ptr);

	/* 4096バイトであること */
	KFS_ASSERT_EQ(4096, size);

	/* 後始末 */
	vfree(ptr);
}

/*
 * テスト: vsize - 複数の割り当て
 * 検証: 複数の割り当てそれぞれのサイズを正しく取得できること
 * 目的: メモリリストからの検索を確認
 */
KFS_TEST(test_vsize_multiple)
{
	void *ptr1, *ptr2, *ptr3;
	size_t size1, size2, size3;

	/* 異なるサイズで割り当て */
	ptr1 = vmalloc(4096);
	ptr2 = vmalloc(8192);
	ptr3 = vmalloc(16384);

	/* サイズを取得 */
	size1 = vsize(ptr1);
	size2 = vsize(ptr2);
	size3 = vsize(ptr3);

	/* 各サイズが正しいこと */
	KFS_ASSERT_EQ(4096, size1);
	KFS_ASSERT_EQ(8192, size2);
	KFS_ASSERT_EQ(16384, size3);

	/* 後始末 */
	vfree(ptr1);
	vfree(ptr2);
	vfree(ptr3);
}

/*
 * テスト: vsize - 不正なアドレス
 * 検証: 不正なアドレスのサイズは0を返すこと
 * 目的: エラーハンドリングを確認
 */
KFS_TEST(test_vsize_invalid_address)
{
	void *invalid_ptr = (void *)0xDEADBEEF;
	size_t size;

	/* 不正なアドレスのサイズを取得 */
	size = vsize(invalid_ptr);

	/* 0が返ること */
	KFS_ASSERT_EQ(0, size);
}

/*
 * テスト: vbrk - スタブ実装
 * 検証: vbrk()はNULLを返すこと（未実装）
 * 目的: スタブ実装の動作を確認
 */
KFS_TEST(test_vbrk_stub)
{
	void *result;

	/* vbrk()を呼び出し */
	result = vbrk(1024);

	/* NULLが返ること（未実装） */
	KFS_ASSERT_TRUE(result == NULL);
}

/*
 * テスト: vmalloc/vfree - 割り当てと解放のサイクル
 * 検証: 割り当てと解放を繰り返しても正常に動作すること
 * 目的: メモリリークがないことを確認
 */
KFS_TEST(test_vmalloc_vfree_cycle)
{
	void *ptr;
	int i;

	/* 10回繰り返す */
	for (i = 0; i < 10; i++)
	{
		/* 割り当て */
		ptr = vmalloc(4096);
		KFS_ASSERT_TRUE(ptr != NULL);

		/* 解放 */
		vfree(ptr);
	}

	/* テスト成功 */
	KFS_ASSERT_TRUE(1);
}

/*
 * テスト: vmalloc/vfree - 順序を変えた解放
 * 検証: 割り当て順と異なる順序で解放しても正常に動作すること
 * 目的: リスト管理の堅牢性を確認
 */
KFS_TEST(test_vmalloc_vfree_out_of_order)
{
	void *ptr1, *ptr2, *ptr3;

	/* 3つの領域を割り当て */
	ptr1 = vmalloc(4096);
	ptr2 = vmalloc(8192);
	ptr3 = vmalloc(16384);

	KFS_ASSERT_TRUE(ptr1 != NULL);
	KFS_ASSERT_TRUE(ptr2 != NULL);
	KFS_ASSERT_TRUE(ptr3 != NULL);

	/* 順序を変えて解放（2→1→3の順） */
	vfree(ptr2);
	vfree(ptr1);
	vfree(ptr3);

	/* テスト成功 */
	KFS_ASSERT_TRUE(1);
}

/*
 * テスト: vmalloc/vfree - 部分的な解放
 * 検証: 複数割り当て後、一部だけ解放しても残りが有効であること
 * 目的: リスト管理の正確性を確認
 */
KFS_TEST(test_vmalloc_vfree_partial)
{
	void *ptr1, *ptr2, *ptr3;
	size_t size1, size3;

	/* 3つの領域を割り当て */
	ptr1 = vmalloc(4096);
	ptr2 = vmalloc(8192);
	ptr3 = vmalloc(16384);

	/* 中央のみ解放 */
	vfree(ptr2);

	/* 残りのサイズが正しいこと */
	size1 = vsize(ptr1);
	size3 = vsize(ptr3);

	KFS_ASSERT_EQ(4096, size1);
	KFS_ASSERT_EQ(16384, size3);

	/* 後始末 */
	vfree(ptr1);
	vfree(ptr3);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_init, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_zero_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_one_page, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_multiple_pages, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_unaligned_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_multiple_allocations, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vfree_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vfree_normal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vfree_invalid_address, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vsize_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vsize_normal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vsize_multiple, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vsize_invalid_address, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vbrk_stub, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_vfree_cycle, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_vfree_out_of_order, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vmalloc_vfree_partial, setup_test, teardown_test),
};

int register_host_tests_vmalloc(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
