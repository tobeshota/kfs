/*
 * mm/memory.c の以下の関数をテスト:
 * - find_vma(): 仮想アドレスを含むVMAを検索
 * - insert_vm_area(): VMAをmm_structのリストに挿入
 * - remove_vm_area(): VMAをmm_structのリストから削除
 * - get_unmapped_area(): カーネル未使用仮想アドレス領域を取得
 */

#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/mm.h>
#include <kfs/mm_types.h>
#include <kfs/stddef.h>
#include <kfs/string.h>

static struct mm_struct test_mm;

static void setup_test(void)
{
	reset_all_state_for_test();
	mm_init(&test_mm, kernel_pgd());
}

static void teardown_test(void)
{
}

KFS_TEST(test_find_vma_empty_list)
{
	struct vm_area_struct *result;

	result = find_vma(&test_mm, 0x10000);
	KFS_ASSERT_TRUE(result == NULL);
}

KFS_TEST(test_insert_vm_area_single)
{
	struct vm_area_struct vma;
	int ret;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ | VM_WRITE;
	vma.vm_next = NULL;

	ret = insert_vm_area(&test_mm, &vma);
	KFS_ASSERT_EQ(0, ret);
	KFS_ASSERT_TRUE(test_mm.mmap != NULL);
	KFS_ASSERT_EQ(0x10000UL, test_mm.mmap->vm_start);
}

KFS_TEST(test_insert_vm_area_multiple_sorted)
{
	struct vm_area_struct vma1, vma2, vma3;
	struct vm_area_struct *current;

	vma1.vm_start = 0x20000;
	vma1.vm_end = 0x30000;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;

	vma2.vm_start = 0x10000;
	vma2.vm_end = 0x20000;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	vma3.vm_start = 0x30000;
	vma3.vm_end = 0x40000;
	vma3.vm_flags = VM_READ;
	vma3.vm_next = NULL;

	insert_vm_area(&test_mm, &vma1);
	insert_vm_area(&test_mm, &vma2);
	insert_vm_area(&test_mm, &vma3);

	current = test_mm.mmap;
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

KFS_TEST(test_find_vma_address_in_range)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ | VM_WRITE;
	vma.vm_next = NULL;

	insert_vm_area(&test_mm, &vma);
	result = find_vma(&test_mm, 0x15000);
	KFS_ASSERT_TRUE(result != NULL);
	KFS_ASSERT_EQ(0x10000UL, result->vm_start);
	KFS_ASSERT_EQ(0x20000UL, result->vm_end);
}

KFS_TEST(test_find_vma_address_out_of_range)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&test_mm, &vma);
	result = find_vma(&test_mm, 0x30000);
	KFS_ASSERT_TRUE(result == NULL);
}

KFS_TEST(test_remove_vm_area_single)
{
	struct vm_area_struct vma;
	struct vm_area_struct *result;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&test_mm, &vma);
	result = find_vma(&test_mm, 0x15000);
	KFS_ASSERT_TRUE(result != NULL);

	result = remove_vm_area(&test_mm, 0x10000);
	KFS_ASSERT_TRUE(result == &vma);

	result = find_vma(&test_mm, 0x15000);
	KFS_ASSERT_TRUE(result == NULL);
}

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

	insert_vm_area(&test_mm, &vma1);
	insert_vm_area(&test_mm, &vma2);
	insert_vm_area(&test_mm, &vma3);

	result = remove_vm_area(&test_mm, 0x20000);
	KFS_ASSERT_TRUE(result == &vma2);

	result = find_vma(&test_mm, 0x15000);
	KFS_ASSERT_TRUE(result != NULL);
	KFS_ASSERT_EQ(0x10000UL, result->vm_start);

	result = find_vma(&test_mm, 0x25000);
	KFS_ASSERT_TRUE(result == NULL);

	result = find_vma(&test_mm, 0x35000);
	KFS_ASSERT_TRUE(result != NULL);
	KFS_ASSERT_EQ(0x30000UL, result->vm_start);
}

KFS_TEST(test_get_unmapped_area_empty_list)
{
	unsigned long addr;

	addr = get_unmapped_area(0x1000);
	KFS_ASSERT_TRUE(addr != 0UL);
}

KFS_TEST(test_get_unmapped_area_user_empty_list)
{
	unsigned long addr;

	addr = get_unmapped_area_user(&test_mm, 0x1000);
	KFS_ASSERT_EQ(0x40000000UL, addr);
}

KFS_TEST(test_get_unmapped_area_user_find_gap)
{
	struct vm_area_struct vma1, vma2;
	unsigned long addr;

	vma1.vm_start = 0x40000000UL;
	vma1.vm_end = 0x40010000UL;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;

	vma2.vm_start = 0x40020000UL;
	vma2.vm_end = 0x40030000UL;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	insert_vm_area(&test_mm, &vma1);
	insert_vm_area(&test_mm, &vma2);

	addr = get_unmapped_area_user(&test_mm, 0x8000);
	KFS_ASSERT_TRUE(addr >= 0x40010000UL && addr < 0x40020000UL);
}

KFS_TEST(test_insert_vm_area_null)
{
	int ret;

	ret = insert_vm_area(&test_mm, NULL);
	KFS_ASSERT_EQ(-1, ret);
}

KFS_TEST(test_remove_vm_area_not_found)
{
	struct vm_area_struct vma;
	struct vm_area_struct *removed;

	vma.vm_start = 0x10000;
	vma.vm_end = 0x20000;
	vma.vm_flags = VM_READ;
	vma.vm_next = NULL;

	insert_vm_area(&test_mm, &vma);
	removed = remove_vm_area(&test_mm, 0x50000);
	KFS_ASSERT_TRUE(removed == NULL);
	KFS_ASSERT_TRUE(find_vma(&test_mm, 0x15000) != NULL);
}

KFS_TEST(test_remove_vm_area_empty_list)
{
	struct vm_area_struct *removed;

	removed = remove_vm_area(&test_mm, 0x10000);
	KFS_ASSERT_TRUE(removed == NULL);
	KFS_ASSERT_TRUE(find_vma(&test_mm, 0x10000) == NULL);
}

KFS_TEST(test_vm_area_lists_are_per_mm)
{
	struct mm_struct other_mm;
	struct vm_area_struct vma1, vma2;

	mm_init(&other_mm, kernel_pgd());
	vma1.vm_start = 0x40000000UL;
	vma1.vm_end = 0x40001000UL;
	vma1.vm_flags = VM_READ;
	vma1.vm_next = NULL;
	vma2.vm_start = 0x40000000UL;
	vma2.vm_end = 0x40001000UL;
	vma2.vm_flags = VM_READ;
	vma2.vm_next = NULL;

	KFS_ASSERT_EQ(0, insert_vm_area(&test_mm, &vma1));
	KFS_ASSERT_EQ(0, insert_vm_area(&other_mm, &vma2));
	KFS_ASSERT_TRUE(find_vma(&test_mm, 0x40000000UL) == &vma1);
	KFS_ASSERT_TRUE(find_vma(&other_mm, 0x40000000UL) == &vma2);

	KFS_ASSERT_TRUE(remove_vm_area(&test_mm, 0x40000000UL) == &vma1);
	KFS_ASSERT_TRUE(find_vma(&test_mm, 0x40000000UL) == NULL);
	KFS_ASSERT_TRUE(find_vma(&other_mm, 0x40000000UL) == &vma2);
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
	KFS_REGISTER_TEST_WITH_SETUP(test_get_unmapped_area_user_empty_list, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_get_unmapped_area_user_find_gap, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_insert_vm_area_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_remove_vm_area_not_found, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_remove_vm_area_empty_list, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_vm_area_lists_are_per_mm, setup_test, teardown_test),
};

int register_unit_tests_memory(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
