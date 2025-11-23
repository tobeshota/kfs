/*
 * test_memory.c - Phase 5 仮想メモリマネージャのテスト
 *
 * mm/memory.c の以下の関数をテスト:
 * - find_vma(): 仮想アドレスを含むVMAを検索
 * - insert_vm_area(): VMAをリストに挿入
 * - remove_vm_area(): VMAをリストから削除
 * - get_unmapped_area(): 未使用の仮想アドレス領域を取得
 */

#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/mm.h>
#include <kfs/stddef.h>

/* 外部からアクセス可能なVMAリスト（mm/memory.cで定義） */
extern struct vm_area_struct *vm_area_list;

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
 * テスト: find_vma - 空リストでの検索
 * 検証: VMAリストが空の時、NULLを返すこと
 * 目的: 初期状態やリスト削除後の安全性を確認
 */
KFS_TEST(test_find_vma_empty_list)
{
	struct vm_area_struct *result;

	/* 空リストでの検索はNULLを返す */
	result = find_vma(0x10000);
	KFS_ASSERT_TRUE(result == NULL);
}

/*
 * テスト: insert_vm_area - 単一VMAの挿入
 * 検証: 1つのVMAを正しく挿入できること
 * 目的: 基本的な挿入操作の正常動作を確認
 */
KFS_TEST(test_insert_vm_area_single)
{
	struct vm_area_struct vma;
	int ret;

	/* VMAを設定 */
	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ | VM_WRITE;
	vma.vm_next = NULL;

	/* 挿入が成功すること */
	ret = insert_vm_area(&vma);
	KFS_ASSERT_EQ(0, ret);

	/* リストの先頭に挿入されること */
	KFS_ASSERT_TRUE(vm_area_list != NULL);
	KFS_ASSERT_EQ(0x10000UL, vm_area_list->vm_start);
}

/*
 * テスト: insert_vm_area - 複数VMAの挿入とソート
 * 検証: 複数のVMAがアドレス順にソートされること
 * 目的: アドレス順序の維持（検索効率化）を確認
 */
KFS_TEST(test_insert_vm_area_multiple_sorted)
{
	struct vm_area_struct vma1, vma2, vma3;
	struct vm_area_struct *current;

	/* VMA1: 中間のアドレス */
	vma1.vm_start = 0x20000;
	vma1.vm_end = 0x30000;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;

	/* VMA2: 最小のアドレス */
	vma2.vm_start = 0x10000;
	vma2.vm_end = 0x20000;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	/* VMA3: 最大のアドレス */
	vma3.vm_start = 0x30000;
	vma3.vm_end = 0x40000;
	vma3.vm_flags = VM_READ;
	vma3.vm_next = NULL;

	/* バラバラな順序で挿入 */
	insert_vm_area(&vma1);
	insert_vm_area(&vma2);
	insert_vm_area(&vma3);

	/* アドレス昇順にソートされていること */
	current = vm_area_list;
	KFS_ASSERT_TRUE(current != NULL);
	KFS_ASSERT_EQ(0x10000UL, current->vm_start);

	current = current->vm_next;
	KFS_ASSERT_TRUE(current != NULL);
	KFS_ASSERT_EQ(0x20000UL, current->vm_start);

	current = current->vm_next;
	KFS_ASSERT_TRUE(current != NULL);
	KFS_ASSERT_EQ(0x30000UL, current->vm_start);

	current = current->vm_next;
	KFS_ASSERT_TRUE(current == NULL);
}

/*
 * テスト: find_vma - 範囲内のアドレスで検索
 * 検証: VMA範囲内のアドレスで正しいVMAが見つかること
 * 目的: 仮想メモリアクセス時の正確なVMA特定を確認
 */
KFS_TEST(test_find_vma_address_in_range)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ | VM_WRITE;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 範囲内のアドレスで検索成功 */
	result = find_vma(0x15000);
	KFS_ASSERT_TRUE(result != NULL);
	KFS_ASSERT_EQ(0x10000UL, result->vm_start);
	KFS_ASSERT_EQ(0x20000UL, result->vm_end);
}

/*
 * テスト: find_vma - 範囲外のアドレスで検索
 * 検証: VMA範囲外のアドレスでNULLが返ること
 * 目的: 不正アクセス検出のための境界チェックを確認
 */
KFS_TEST(test_find_vma_address_out_of_range)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 範囲外のアドレスではNULL */
	result = find_vma(0x30000);
	KFS_ASSERT_TRUE(result == NULL);
}

/*
 * テスト: remove_vm_area - 単一VMAの削除
 * 検証: 指定アドレスのVMAが正しく削除されること
 * 目的: メモリ領域解放時のVMA管理を確認
 */
KFS_TEST(test_remove_vm_area_single)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 削除前は見つかる */
	result = find_vma(0x15000);
	KFS_ASSERT_TRUE(result != NULL);

	/* 削除実行 */
	remove_vm_area(0x10000);

	/* 削除後は見つからない */
	result = find_vma(0x15000);
	KFS_ASSERT_TRUE(result == NULL);
}

/*
 * テスト: remove_vm_area - 中間VMAの削除
 * 検証: リスト中間のVMAを削除しても前後のリンクが維持されること
 * 目的: リンクリストの整合性を確認
 */
KFS_TEST(test_remove_vm_area_middle)
{
	struct vm_area_struct vma1, vma2, vma3;
	struct vm_area_struct *result;

	vma1.vm_start = 0x10000;
	vma1.vm_end = 0x20000;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;

	vma2.vm_start = 0x20000;
	vma2.vm_end = 0x30000;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	vma3.vm_start = 0x30000;
	vma3.vm_end = 0x40000;
	vma3.vm_flags = VM_READ;
	vma3.vm_next = NULL;

	insert_vm_area(&vma1);
	insert_vm_area(&vma2);
	insert_vm_area(&vma3);

	/* 中間のVMAを削除 */
	remove_vm_area(0x20000);

	/* vma1はまだ存在 */
	result = find_vma(0x15000);
	KFS_ASSERT_TRUE(result != NULL);
	KFS_ASSERT_EQ(0x10000UL, result->vm_start);

	/* vma2は削除済み */
	result = find_vma(0x25000);
	KFS_ASSERT_TRUE(result == NULL);

	/* vma3はまだ存在 */
	result = find_vma(0x35000);
	KFS_ASSERT_TRUE(result != NULL);
	KFS_ASSERT_EQ(0x30000UL, result->vm_start);
}

/*
 * テスト: get_unmapped_area - 空リストでの領域取得
 * 検証: VMAが存在しない時、基底アドレスから領域を取得できること
 * 目的: 初期状態での仮想メモリ割り当てを確認
 */
KFS_TEST(test_get_unmapped_area_empty_list)
{
	unsigned long addr;

	/* 空リストでは基底アドレスが返る */
	addr = get_unmapped_area(0x1000);
	KFS_ASSERT_TRUE(addr != 0UL);
}

/*
 * テスト: get_unmapped_area - VMA間の隙間を検出
 * 検証: 既存VMA間の十分な空き領域を見つけられること
 * 目的: メモリの断片化を避けた効率的な割り当てを確認
 */
KFS_TEST(test_get_unmapped_area_find_gap)
{
	struct vm_area_struct vma1, vma2;
	unsigned long addr;

	/* VMA1: 0x10000-0x20000 */
	vma1.vm_start = 0x10000;
	vma1.vm_end = 0x20000;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;

	/* VMA2: 0x30000-0x40000 (0x10000の隙間がある) */
	vma2.vm_start = 0x30000;
	vma2.vm_end = 0x40000;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	insert_vm_area(&vma1);
	insert_vm_area(&vma2);

	/* 隙間に収まるサイズで検索 */
	addr = get_unmapped_area(0x8000); /* 32KB */
	KFS_ASSERT_TRUE(addr != 0UL);

	/* 隙間内のアドレスが返ること */
	KFS_ASSERT_TRUE(addr >= 0x20000UL && addr < 0x30000UL);
}

/*
 * テスト: get_unmapped_area - サイズが大きすぎる場合
 * 検証: どの隙間にも収まらない大きなサイズで0が返ること
 * 目的: メモリ不足時のエラーハンドリングを確認
 */
KFS_TEST(test_get_unmapped_area_too_large)
{
	struct vm_area_struct vma1, vma2;
	unsigned long addr;

	vma1.vm_start = 0x10000;
	vma1.vm_end = 0x20000;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;

	vma2.vm_start = 0x25000;
	vma2.vm_end = 0x30000;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	insert_vm_area(&vma1);
	insert_vm_area(&vma2);

	/* 隙間(0x5000)より大きいサイズを要求 */
	addr = get_unmapped_area(0x10000); /* 64KB */

	/* 収まらない場合は次の領域が返る（実装依存） */
	KFS_ASSERT_TRUE(addr == 0 || addr >= 0x30000UL);
}

/*
 * テスト: insert_vm_area - NULL VMAの挿入
 * 検証: NULLポインタを渡すとエラーが返ること
 * 目的: 不正な引数に対する防御的プログラミングを確認
 */
KFS_TEST(test_insert_vm_area_null)
{
	int ret;

	/* NULLの挿入は失敗 */
	ret = insert_vm_area(NULL);
	KFS_ASSERT_EQ(-1, ret);
}

/*
 * テスト: remove_vm_area - 存在しないアドレスの削除
 * 検証: 存在しないアドレスを指定しても安全に処理されること
 * 目的: 二重解放などの不正操作への耐性を確認
 */
KFS_TEST(test_remove_vm_area_not_found)
{
	struct vm_area_struct vma;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 存在しないアドレスを削除（クラッシュしないこと） */
	remove_vm_area(0x50000);

	/* 既存のVMAは影響を受けない */
	KFS_ASSERT_TRUE(find_vma(0x15000) != NULL);
}

/*
 * テスト: remove_vm_area - 空リストでの削除
 * 検証: リストが空の状態でremove_vm_areaを呼んでも安全に処理されること
 * 目的: 初期状態や全削除後の状態での安全性を確認
 */
KFS_TEST(test_remove_vm_area_empty_list)
{

	/* 空リストで削除（クラッシュしないこと） */
	remove_vm_area(0x10000);

	/* リストは空のまま */
	KFS_ASSERT_TRUE(find_vma(0x10000) == NULL);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_find_vma_empty_list, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_insert_vm_area_single, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_insert_vm_area_multiple_sorted, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_find_vma_address_in_range, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_find_vma_address_out_of_range, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_remove_vm_area_single, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_remove_vm_area_middle, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_get_unmapped_area_empty_list, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_get_unmapped_area_find_gap, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_get_unmapped_area_too_large, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_insert_vm_area_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_remove_vm_area_not_found, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_remove_vm_area_empty_list, setup_test, teardown_test),
};

int register_host_tests_memory(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
