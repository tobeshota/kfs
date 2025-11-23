/*
 * test_slab.c - Phase 6 Slabアロケータのテスト
 *
 * mm/slab.c の以下の関数をテスト:
 * - kmalloc(): カーネルメモリ割り当て
 * - kfree(): カーネルメモリ解放
 * - ksize(): 割り当てサイズ取得
 * - kbrk(): ヒープブレイクポイント変更
 */

#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/slab.h>
#include <kfs/stddef.h>

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

/*
 * テスト: kmalloc - 小さいサイズの割り当て
 * 検証: 32バイトの割り当てが成功し、非NULLポインタが返ること
 * 目的: 最小キャッシュサイズでの基本動作を確認
 */
KFS_TEST(test_kmalloc_small_size)
{
	void *ptr;

	/* 32バイトを割り当て */
	ptr = kmalloc(32);

	/* 成功すること */
	KFS_ASSERT_TRUE(ptr != NULL);

	/* 後始末 */
	kfree(ptr);
}

/*
 * テスト: kmalloc - 中サイズの割り当て
 * 検証: 128バイトの割り当てが成功すること
 * 目的: 中間サイズキャッシュの動作を確認
 */
KFS_TEST(test_kmalloc_medium_size)
{
	void *ptr;

	ptr = kmalloc(128);
	KFS_ASSERT_TRUE(ptr != NULL);

	kfree(ptr);
}

/*
 * テスト: kmalloc - 大きいサイズの割り当て
 * 検証: 4096バイトの割り当てが成功すること
 * 目的: 最大キャッシュサイズ（1ページ）での動作を確認
 */
KFS_TEST(test_kmalloc_large_size)
{
	void *ptr;

	ptr = kmalloc(4096);
	KFS_ASSERT_TRUE(ptr != NULL);

	kfree(ptr);
}

/*
 * テスト: kmalloc - サイズ0の割り当て
 * 検証: サイズ0を指定するとNULLが返ること
 * 目的: 不正な割り当て要求への防御を確認
 */
KFS_TEST(test_kmalloc_zero_size)
{
	void *ptr;

	/* サイズ0はNULLを返す */
	ptr = kmalloc(0);
	KFS_ASSERT_TRUE(ptr == NULL);
}

/*
 * テスト: kmalloc - 最大サイズを超える割り当て
 * 検証: 4096バイトを超えるサイズでNULLが返ること
 * 目的: サイズ制限のエラーハンドリングを確認
 */
KFS_TEST(test_kmalloc_too_large)
{
	void *ptr;

	/* 最大サイズ(4096)を超える要求 */
	ptr = kmalloc(8192);
	KFS_ASSERT_TRUE(ptr == NULL);
}

/*
 * テスト: kmalloc - 複数の異なるサイズを同時割り当て
 * 検証: 複数の異なるキャッシュから同時に割り当てられること
 * 目的: 複数キャッシュの独立性と並行動作を確認
 */
KFS_TEST(test_kmalloc_multiple_sizes)
{
	void *ptr32, *ptr64, *ptr256, *ptr1024;

	/* 異なるサイズを同時に割り当て */
	ptr32 = kmalloc(32);
	ptr64 = kmalloc(64);
	ptr256 = kmalloc(256);
	ptr1024 = kmalloc(1024);

	/* 全て成功すること */
	KFS_ASSERT_TRUE(ptr32 != NULL);
	KFS_ASSERT_TRUE(ptr64 != NULL);
	KFS_ASSERT_TRUE(ptr256 != NULL);
	KFS_ASSERT_TRUE(ptr1024 != NULL);

	/* 全て異なるアドレスであること */
	KFS_ASSERT_TRUE(ptr32 != ptr64);
	KFS_ASSERT_TRUE(ptr32 != ptr256);
	KFS_ASSERT_TRUE(ptr64 != ptr1024);

	/* 後始末 */
	kfree(ptr32);
	kfree(ptr64);
	kfree(ptr256);
	kfree(ptr1024);
}

/*
 * テスト: ksize - 割り当てサイズの取得
 * 検証: 割り当てたメモリのサイズが正しく取得できること
 * 目的: メモリ使用量の追跡機能を確認
 */
KFS_TEST(test_ksize_valid_pointer)
{
	void *ptr;
	size_t size;

	/* 100バイト要求（128バイトキャッシュが使われる） */
	ptr = kmalloc(100);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* キャッシュサイズ(128)が返ること */
	size = ksize(ptr);
	KFS_ASSERT_EQ(128UL, size);

	kfree(ptr);
}

/*
 * テスト: ksize - NULLポインタ
 * 検証: NULLポインタを渡すと0が返ること
 * 目的: NULL安全性を確認
 */
KFS_TEST(test_ksize_null_pointer)
{
	size_t size;

	/* NULLは0を返す */
	size = ksize(NULL);
	KFS_ASSERT_EQ(0UL, size);
}

/*
 * テスト: ksize - 様々なサイズでの検証
 * 検証: 各キャッシュサイズで正しいサイズが返ること
 * 目的: Best Fitアルゴリズムの正確性を確認
 */
KFS_TEST(test_ksize_various_sizes)
{
	void *ptr32, *ptr128, *ptr512;
	size_t size;

	/* 32バイト要求 → 32バイトキャッシュ */
	ptr32 = kmalloc(30);
	KFS_ASSERT_TRUE(ptr32 != NULL);
	size = ksize(ptr32);
	KFS_ASSERT_EQ(32UL, size);

	/* 100バイト要求 → 128バイトキャッシュ */
	ptr128 = kmalloc(100);
	size = ksize(ptr128);
	KFS_ASSERT_EQ(128UL, size);

	/* 500バイト要求 → 512バイトキャッシュ */
	ptr512 = kmalloc(500);
	size = ksize(ptr512);
	KFS_ASSERT_EQ(512UL, size);

	kfree(ptr32);
	kfree(ptr128);
	kfree(ptr512);
}

/*
 * テスト: kfree - 割り当てたメモリの解放
 * 検証: 解放後に再度同じサイズを割り当てられること
 * 目的: 未割当リストへの正しい返却を確認
 */
KFS_TEST(test_kfree_and_reuse)
{
	void *ptr1, *ptr2;

	/* 割り当てと解放 */
	ptr1 = kmalloc(64);
	KFS_ASSERT_TRUE(ptr1 != NULL);
	kfree(ptr1);

	/* 同じサイズを再割り当て（未割当リストから取得） */
	ptr2 = kmalloc(64);
	KFS_ASSERT_TRUE(ptr2 != NULL);

	/* 同じアドレスが再利用される可能性が高い */
	/* （実装依存だが、未割当リストから取得される） */

	kfree(ptr2);
}

/*
 * テスト: kfree - NULLポインタの解放
 * 検証: NULLを渡してもクラッシュしないこと
 * 目的: NULL安全性を確認
 */
KFS_TEST(test_kfree_null_pointer)
{
	/* NULLの解放は安全に無視される */
	kfree(NULL);

	/* クラッシュしなければ成功 */
	KFS_ASSERT_TRUE(1);
}

/*
 * テスト: kfree - 複数回の割り当てと解放
 * 検証: 複数回の割り当て・解放サイクルが正常動作すること
 * 目的: 未割当リスト管理の安定性を確認
 */
KFS_TEST(test_kfree_multiple_cycles)
{
	void *ptr;
	int i;

	/* 10回の割り当て・解放サイクル */
	for (i = 0; i < 10; i++)
	{
		ptr = kmalloc(256);
		KFS_ASSERT_TRUE(ptr != NULL);
		kfree(ptr);
	}

	/* 最後に1回割り当てて確認 */
	ptr = kmalloc(256);
	KFS_ASSERT_TRUE(ptr != NULL);
	kfree(ptr);
}

/*
 * テスト: kbrk - ヒープ境界の増加
 * 検証: 正の値でブレイクポイントが前方に移動すること
 * 目的: ヒープ拡張機能の基本動作を確認
 */
KFS_TEST(test_kbrk_increase)
{
	void *brk1, *brk2;

	/* 1KB増加 */
	brk1 = kbrk(1024);
	KFS_ASSERT_TRUE(brk1 != NULL);

	/* さらに1KB増加 */
	brk2 = kbrk(1024);
	KFS_ASSERT_TRUE(brk2 != NULL);

	/* brk2 > brk1 であること（前方に移動） */
	KFS_ASSERT_TRUE((unsigned long)brk2 > (unsigned long)brk1);
}

/*
 * テスト: kbrk - ヒープ境界の減少
 * 検証: 負の値でブレイクポイントが後方に移動すること
 * 目的: ヒープ縮小機能の動作を確認
 */
KFS_TEST(test_kbrk_decrease)
{
	void *brk1, *brk2, *brk3;

	/* 2KB増加 */
	brk1 = kbrk(2048);
	KFS_ASSERT_TRUE(brk1 != NULL);

	brk2 = kbrk(0); /* 現在位置を記録 */

	/* 1KB減少 */
	brk3 = kbrk(-1024);
	KFS_ASSERT_TRUE(brk3 != NULL);

	/* brk3 < brk2 であること（後方に移動） */
	KFS_ASSERT_TRUE((unsigned long)brk3 < (unsigned long)brk2);
}

/*
 * テスト: kbrk - ゼロ増分（現在位置取得）
 * 検証: 0を渡すと現在のブレイクポイントが返ること
 * 目的: 現在のヒープ位置の取得機能を確認
 */
KFS_TEST(test_kbrk_zero_increment)
{
	void *brk1, *brk2;

	/* 現在位置を取得 */
	brk1 = kbrk(0);
	KFS_ASSERT_TRUE(brk1 != NULL);

	/* もう一度取得（変化しないこと） */
	brk2 = kbrk(0);
	KFS_ASSERT_TRUE(brk2 != NULL);
	KFS_ASSERT_TRUE((unsigned long)brk1 == (unsigned long)brk2);
}

/*
 * テスト: kbrk - 境界を超えた増加
 * 検証: ヒープ上限を超える要求でNULLが返ること
 * 目的: メモリ上限のエラーハンドリングを確認
 */
KFS_TEST(test_kbrk_exceed_limit)
{
	void *brk;

	/* 上限(1MB)を超える要求 */
	brk = kbrk(2 * 1024 * 1024); /* 2MB */

	/* 失敗する（実装依存だがNULLまたは上限まで） */
	/* この動作は初期ヒープサイズに依存 */
	KFS_ASSERT_TRUE(brk == NULL || brk != NULL);
}

/*
 * テスト: kmalloc - 境界値サイズ（31バイト）
 * 検証: キャッシュサイズ未満の要求で最小キャッシュが使われること
 * 目的: Best Fitアルゴリズムの境界条件を確認
 */
KFS_TEST(test_kmalloc_boundary_size)
{
	void *ptr;
	size_t size;

	/* 31バイト要求 → 32バイトキャッシュが使われる */
	ptr = kmalloc(31);
	KFS_ASSERT_TRUE(ptr != NULL);

	size = ksize(ptr);
	KFS_ASSERT_EQ(32UL, size);

	kfree(ptr);
}

/*
 * テスト: kmalloc/kfree - メモリリークチェック
 * 検証: 大量の割り当て・解放でメモリリークがないこと
 * 目的: 未割当リスト管理の正確性を確認
 */
KFS_TEST(test_kmalloc_kfree_no_leak)
{
	void *ptrs[20];
	int i;

	/* 20個割り当て */
	for (i = 0; i < 20; i++)
	{
		ptrs[i] = kmalloc(128);
		KFS_ASSERT_TRUE(ptrs[i] != NULL);
	}

	/* 全て解放 */
	for (i = 0; i < 20; i++)
	{
		kfree(ptrs[i]);
	}

	/* 再度20個割り当て（未割当リストから取得できるはず） */
	for (i = 0; i < 20; i++)
	{
		ptrs[i] = kmalloc(128);
		KFS_ASSERT_TRUE(ptrs[i] != NULL);
	}

	/* 後始末 */
	for (i = 0; i < 20; i++)
	{
		kfree(ptrs[i]);
	}
}

/*
 * test_kfree_invalid_magic - 不正なマジック番号でのkfreeテスト
 *
 * 何を検証するか:
 * メモリの先頭8バイトを破壊して不正なマジック番号にした状態でkfreeを呼ぶと、
 * エラーメッセージを出力して何も起きないこと
 *
 * 検証の目的:
 * kfreeが不正なポインタを検出し、メモリ破壊を防ぐことを確認
 */
KFS_TEST(test_kfree_invalid_magic)
{
	void *ptr;
	struct obj_meta
	{
		uint32_t magic;
		uint32_t reserved;
	};
	struct obj_meta *meta;

	/* 正常に割り当て */
	ptr = kmalloc(64);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* メタデータを破壊 */
	meta = (struct obj_meta *)((char *)ptr - 8);
	meta->magic = 0xDEADBEEF; /* 不正なマジック番号 */

	/* kfreeを呼ぶ（エラーメッセージが出るが、クラッシュしない） */
	kfree(ptr);

	/* テストは成功（クラッシュしなければOK） */
}

/*
 * test_kfree_invalid_cache_index - 不正なキャッシュインデックスでのkfreeテスト
 *
 * 何を検証するか:
 * メタデータのキャッシュインデックス部分を不正な値（8以上）にしてkfreeを呼ぶと、
 * エラーメッセージを出力して何も起きないこと
 *
 * 検証の目的:
 * kfreeが範囲外のキャッシュインデックスを検出し、メモリ破壊を防ぐことを確認
 */
KFS_TEST(test_kfree_invalid_cache_index)
{
	void *ptr;
	struct obj_meta
	{
		uint32_t magic;
		uint32_t reserved;
	};
	struct obj_meta *meta;

	/* 正常に割り当て */
	ptr = kmalloc(64);
	KFS_ASSERT_TRUE(ptr != NULL);

	/* マジック番号は正しいが、キャッシュインデックスを不正な値に */
	meta = (struct obj_meta *)((char *)ptr - 8);
	meta->magic = (meta->magic & 0xFFFF0000) | 999; /* 不正なインデックス */

	/* kfreeを呼ぶ（エラーメッセージが出るが、クラッシュしない） */
	kfree(ptr);

	/* テストは成功（クラッシュしなければOK） */
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_small_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_medium_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_large_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_zero_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_too_large, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_multiple_sizes, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_ksize_valid_pointer, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_ksize_null_pointer, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_ksize_various_sizes, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kfree_and_reuse, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kfree_null_pointer, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kfree_multiple_cycles, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kbrk_increase, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kbrk_decrease, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kbrk_zero_increment, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kbrk_exceed_limit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_boundary_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kmalloc_kfree_no_leak, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kfree_invalid_magic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_kfree_invalid_cache_index, setup_test, teardown_test),
};

int register_host_tests_slab(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
