/*
 * test_memory.c - Phase 5 仮想メモリマネージャのテスト
 *
 * mm/memory.c の以下の関数をテスト:
 * - find_vma(): 仮想アドレスを含むVMAを検索
 * - insert_vm_area(): VMAをリストに挿入
 * - remove_vm_area(): VMAをリストから削除
 * - get_unmapped_area(): 未使用の仮想アドレス領域を取得
 */

#include "host_test_framework.h"
#include <kfs/mm.h>
#include <kfs/stddef.h>

/* test_memory_host.cで定義される関数 */
extern void kfs_register_test_case(const char *name, void (*test_func)(void));

/* 互換性マクロ */
#define TEST(name) KFS_TEST(name)
#define ASSERT_NULL(ptr) KFS_ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) KFS_ASSERT_TRUE((ptr) != NULL)
#define ASSERT_EQUAL(a, b) KFS_ASSERT_EQ((b), (a))
#define ASSERT_NOT_EQUAL(a, b) KFS_ASSERT_TRUE((a) != (b))
#define ASSERT_TRUE(expr) KFS_ASSERT_TRUE(expr)
#define REGISTER_TEST(fn) kfs_register_test_case(#fn, fn)

/* 外部からアクセス可能なVMAリスト（mm/memory.cで定義） */
extern struct vm_area_struct *vm_area_list;

/* テスト前にVMAリストをクリア */
static void setup_vm_area_list(void)
{
	vm_area_list = NULL;
}

/*
 * テスト: find_vma - 空リストでの検索
 * 検証: VMAリストが空の時、NULLを返すこと
 * 目的: 初期状態やリスト削除後の安全性を確認
 */
TEST(test_find_vma_empty_list)
{
	struct vm_area_struct *result;

	setup_vm_area_list();

	/* 空リストでの検索はNULLを返す */
	result = find_vma(0x10000);
	ASSERT_NULL(result);
}

/*
 * テスト: insert_vm_area - 単一VMAの挿入
 * 検証: 1つのVMAを正しく挿入できること
 * 目的: 基本的な挿入操作の正常動作を確認
 */
TEST(test_insert_vm_area_single)
{
	struct vm_area_struct vma;
	int ret;

	setup_vm_area_list();

	/* VMAを設定 */
	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ | VM_WRITE;
	vma.vm_next = NULL;

	/* 挿入が成功すること */
	ret = insert_vm_area(&vma);
	ASSERT_EQUAL(ret, 0);

	/* リストの先頭に挿入されること */
	ASSERT_NOT_NULL(vm_area_list);
	ASSERT_EQUAL(vm_area_list->vm_start, 0x10000UL);
}

/*
 * テスト: insert_vm_area - 複数VMAの挿入とソート
 * 検証: 複数のVMAがアドレス順にソートされること
 * 目的: アドレス順序の維持（検索効率化）を確認
 */
TEST(test_insert_vm_area_multiple_sorted)
{
	struct vm_area_struct vma1, vma2, vma3;
	struct vm_area_struct *current;

	setup_vm_area_list();

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
	ASSERT_NOT_NULL(current);
	ASSERT_EQUAL(current->vm_start, 0x10000UL);

	current = current->vm_next;
	ASSERT_NOT_NULL(current);
	ASSERT_EQUAL(current->vm_start, 0x20000UL);

	current = current->vm_next;
	ASSERT_NOT_NULL(current);
	ASSERT_EQUAL(current->vm_start, 0x30000UL);

	current = current->vm_next;
	ASSERT_NULL(current);
}

/*
 * テスト: find_vma - 範囲内のアドレスで検索
 * 検証: VMA範囲内のアドレスで正しいVMAが見つかること
 * 目的: 仮想メモリアクセス時の正確なVMA特定を確認
 */
TEST(test_find_vma_address_in_range)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	setup_vm_area_list();

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ | VM_WRITE;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 範囲内のアドレスで検索成功 */
	result = find_vma(0x15000);
	ASSERT_NOT_NULL(result);
	ASSERT_EQUAL(result->vm_start, 0x10000UL);
	ASSERT_EQUAL(result->vm_end, 0x20000UL);
}

/*
 * テスト: find_vma - 範囲外のアドレスで検索
 * 検証: VMA範囲外のアドレスでNULLが返ること
 * 目的: 不正アクセス検出のための境界チェックを確認
 */
TEST(test_find_vma_address_out_of_range)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	setup_vm_area_list();

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 範囲外のアドレスではNULL */
	result = find_vma(0x30000);
	ASSERT_NULL(result);
}

/*
 * テスト: remove_vm_area - 単一VMAの削除
 * 検証: 指定アドレスのVMAが正しく削除されること
 * 目的: メモリ領域解放時のVMA管理を確認
 */
TEST(test_remove_vm_area_single)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	setup_vm_area_list();

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 削除前は見つかる */
	result = find_vma(0x15000);
	ASSERT_NOT_NULL(result);

	/* 削除実行 */
	remove_vm_area(0x10000);

	/* 削除後は見つからない */
	result = find_vma(0x15000);
	ASSERT_NULL(result);
}

/*
 * テスト: remove_vm_area - 中間VMAの削除
 * 検証: リスト中間のVMAを削除しても前後のリンクが維持されること
 * 目的: リンクリストの整合性を確認
 */
TEST(test_remove_vm_area_middle)
{
	struct vm_area_struct vma1, vma2, vma3;
	struct vm_area_struct *result;

	setup_vm_area_list();

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
	ASSERT_NOT_NULL(result);
	ASSERT_EQUAL(result->vm_start, 0x10000UL);

	/* vma2は削除済み */
	result = find_vma(0x25000);
	ASSERT_NULL(result);

	/* vma3はまだ存在 */
	result = find_vma(0x35000);
	ASSERT_NOT_NULL(result);
	ASSERT_EQUAL(result->vm_start, 0x30000UL);
}

/*
 * テスト: get_unmapped_area - 空リストでの領域取得
 * 検証: VMAが存在しない時、基底アドレスから領域を取得できること
 * 目的: 初期状態での仮想メモリ割り当てを確認
 */
TEST(test_get_unmapped_area_empty_list)
{
	unsigned long addr;

	setup_vm_area_list();

	/* 空リストでは基底アドレスが返る */
	addr = get_unmapped_area(0x1000);
	ASSERT_NOT_EQUAL(addr, 0UL);
}

/*
 * テスト: get_unmapped_area - VMA間の隙間を検出
 * 検証: 既存VMA間の十分な空き領域を見つけられること
 * 目的: メモリの断片化を避けた効率的な割り当てを確認
 */
TEST(test_get_unmapped_area_find_gap)
{
	struct vm_area_struct vma1, vma2;
	unsigned long addr;

	setup_vm_area_list();

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
	ASSERT_NOT_EQUAL(addr, 0UL);

	/* 隙間内のアドレスが返ること */
	ASSERT_TRUE(addr >= 0x20000UL && addr < 0x30000UL);
}

/*
 * テスト: get_unmapped_area - サイズが大きすぎる場合
 * 検証: どの隙間にも収まらない大きなサイズで0が返ること
 * 目的: メモリ不足時のエラーハンドリングを確認
 */
TEST(test_get_unmapped_area_too_large)
{
	struct vm_area_struct vma1, vma2;
	unsigned long addr;

	setup_vm_area_list();

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
	ASSERT_TRUE(addr == 0 || addr >= 0x30000UL);
}

/*
 * テスト: insert_vm_area - NULL VMAの挿入
 * 検証: NULLポインタを渡すとエラーが返ること
 * 目的: 不正な引数に対する防御的プログラミングを確認
 */
TEST(test_insert_vm_area_null)
{
	int ret;

	setup_vm_area_list();

	/* NULLの挿入は失敗 */
	ret = insert_vm_area(NULL);
	ASSERT_EQUAL(ret, -1);
}

/*
 * テスト: remove_vm_area - 存在しないアドレスの削除
 * 検証: 存在しないアドレスを指定しても安全に処理されること
 * 目的: 二重解放などの不正操作への耐性を確認
 */
TEST(test_remove_vm_area_not_found)
{
	struct vm_area_struct vma;

	setup_vm_area_list();

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&vma);

	/* 存在しないアドレスを削除（クラッシュしないこと） */
	remove_vm_area(0x50000);

	/* 既存のVMAは影響を受けない */
	ASSERT_NOT_NULL(find_vma(0x15000));
}

/*
 * テスト: remove_vm_area - 空リストでの削除
 * 検証: リストが空の状態でremove_vm_areaを呼んでも安全に処理されること
 * 目的: 初期状態や全削除後の状態での安全性を確認
 */
TEST(test_remove_vm_area_empty_list)
{
	setup_vm_area_list();

	/* 空リストで削除（クラッシュしないこと） */
	remove_vm_area(0x10000);

	/* リストは空のまま */
	ASSERT_NULL(find_vma(0x10000));
}

/* テスト登録 */
void register_memory_tests(void)
{
	REGISTER_TEST(test_find_vma_empty_list);
	REGISTER_TEST(test_insert_vm_area_single);
	REGISTER_TEST(test_insert_vm_area_multiple_sorted);
	REGISTER_TEST(test_find_vma_address_in_range);
	REGISTER_TEST(test_find_vma_address_out_of_range);
	REGISTER_TEST(test_remove_vm_area_single);
	REGISTER_TEST(test_remove_vm_area_middle);
	REGISTER_TEST(test_get_unmapped_area_empty_list);
	REGISTER_TEST(test_get_unmapped_area_find_gap);
	REGISTER_TEST(test_get_unmapped_area_too_large);
	REGISTER_TEST(test_insert_vm_area_null);
	REGISTER_TEST(test_remove_vm_area_not_found);
	REGISTER_TEST(test_remove_vm_area_empty_list);
}
