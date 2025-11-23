/**
 * テスト用: 共通リセット関数
 *
 * 全テストの独立性を保証するために、各テスト前に呼び出すリセット関数を提供する。
 */

#include <kfs/mm.h>
#include <kfs/slab.h>

/**
 * 全サブシステムを初期状態にリセット
 * @details
 * 全テストで統一的に使用するリセット関数。
 * テストの独立性を保証するために、各テスト前に呼び出す。
 *
 * リセット対象:
 * - 仮想メモリ領域（VMA）
 * - ページアロケータ
 * - Slabアロケータ
 */
void reset_all_state_for_test(void)
{
	/* VMAをリセット（依存関係: Slabの前にクリア） */
	vm_reset_for_test();

	/* ページアロケータをリセット */
	page_allocator_reset_for_test();

	/* Slabアロケータをリセット（ページアロケータに依存） */
	kmem_cache_reset_for_test();
}
