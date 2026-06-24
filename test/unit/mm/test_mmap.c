/*
 * mm/mmap.c の以下の関数をテスト:
 * - do_mmap(): currentの匿名メモリマッピングの確保
 * - do_mmap_mm(): 指定mm_structの匿名メモリマッピングの確保
 * - do_munmap(): メモリマッピングの解放
 */

#include "../test_reset.h"
#include "unit_test_framework.h"
#include <asm-i386/pgtable.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/mm_types.h>
#include <kfs/mman.h>
#include <kfs/sched.h>
#include <kfs/stddef.h>

static struct mm_struct test_mm;

static void setup_test(void)
{
	reset_all_state_for_test();
	mm_init(&test_mm, kernel_pgd());
	current->mm = &test_mm;
}

static void teardown_test(void)
{
	current->mm = NULL;
}

KFS_TEST(test_do_mmap_returns_valid_address)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE(addr != NULL);
	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);
}

KFS_TEST(test_do_mmap_inserts_vma)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	struct vm_area_struct *vma;

	KFS_ASSERT_TRUE(addr != NULL);
	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);

	vma = find_vma(current->mm, (unsigned long)addr);
	KFS_ASSERT_TRUE(vma != NULL);
	KFS_ASSERT_EQ(vma->vm_start, (unsigned long)addr);
}

KFS_TEST(test_do_mmap_write_readable)
{
	unsigned char *buf = (unsigned char *)do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)buf != (unsigned long)MAP_FAILED);

	buf[0] = 0xAB;
	buf[4095] = 0xCD;
	KFS_ASSERT_EQ((unsigned char)0xAB, (unsigned char)buf[0]);
	KFS_ASSERT_EQ((unsigned char)0xCD, (unsigned char)buf[4095]);
}

KFS_TEST(test_do_munmap_removes_vma)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	int ret;
	struct vm_area_struct *vma;

	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);

	ret = do_munmap((unsigned long)addr, 4096);
	KFS_ASSERT_EQ(0, ret);

	vma = find_vma(current->mm, (unsigned long)addr);
	KFS_ASSERT_TRUE(vma == NULL);
}

KFS_TEST(test_do_mmap_zero_len_fails)
{
	void *addr = do_mmap(NULL, 0, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)addr == (unsigned long)MAP_FAILED);
}

KFS_TEST(test_do_mmap_no_anonymous_fails)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)addr == (unsigned long)MAP_FAILED);
}

KFS_TEST(test_do_mmap_multiple_non_overlapping)
{
	void *a1 = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	void *a2 = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)a1 != (unsigned long)MAP_FAILED);
	KFS_ASSERT_TRUE((unsigned long)a2 != (unsigned long)MAP_FAILED);
	KFS_ASSERT_TRUE((unsigned long)a1 != (unsigned long)a2);
}

KFS_TEST(test_do_munmap_invalid_addr)
{
	int ret = do_munmap(0xDEAD0000UL, 4096);
	KFS_ASSERT_EQ(-1, ret);
}

KFS_TEST(test_sys_mmap2_wrapper)
{
	extern void *sys_mmap2(unsigned long addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff);
	void *addr = sys_mmap2(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);
	KFS_ASSERT_TRUE(addr != NULL);
}

KFS_TEST(test_sys_munmap_wrapper)
{
	extern int sys_munmap(unsigned long addr, unsigned long len);
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	int ret;

	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);
	ret = sys_munmap((unsigned long)addr, 4096);
	KFS_ASSERT_EQ(0, ret);
}

KFS_TEST(test_do_mmap_prot_read_only)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE);
	struct vm_area_struct *vma;

	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);
	vma = find_vma(current->mm, (unsigned long)addr);
	KFS_ASSERT_TRUE(vma != NULL);
}

KFS_TEST(test_do_mmap_munmap_remap_cycle)
{
	void *a1 = do_mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	void *a2;
	int ret;

	KFS_ASSERT_TRUE((unsigned long)a1 != (unsigned long)MAP_FAILED);
	ret = do_munmap((unsigned long)a1, PAGE_SIZE);
	KFS_ASSERT_EQ(0, ret);
	a2 = do_mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)a2 != (unsigned long)MAP_FAILED);
}

KFS_TEST(test_do_mmap_mm_isolates_vma_lists)
{
	struct mm_struct first;
	struct mm_struct second;
	struct page *first_pgd;
	struct page *second_pgd;
	void *first_addr;
	void *second_addr;

	first_pgd = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
	second_pgd = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
	KFS_ASSERT_TRUE(first_pgd != NULL);
	KFS_ASSERT_TRUE(second_pgd != NULL);
	KFS_ASSERT_EQ(0, copy_page_tables((pgd_t *)first_pgd, kernel_pgd()));
	KFS_ASSERT_EQ(0, copy_page_tables((pgd_t *)second_pgd, kernel_pgd()));
	mm_init(&first, (pgd_t *)first_pgd);
	mm_init(&second, (pgd_t *)second_pgd);

	first_addr = do_mmap_mm(&first, NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	second_addr = do_mmap_mm(&second, NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)first_addr != (unsigned long)MAP_FAILED);
	KFS_ASSERT_TRUE((unsigned long)second_addr != (unsigned long)MAP_FAILED);
	KFS_ASSERT_EQ((unsigned long)first_addr, (unsigned long)second_addr);

	KFS_ASSERT_TRUE(find_vma(&first, (unsigned long)first_addr) != NULL);
	KFS_ASSERT_TRUE(find_vma(&second, (unsigned long)second_addr) != NULL);
	KFS_ASSERT_EQ(0, do_munmap_mm(&first, (unsigned long)first_addr, PAGE_SIZE));
	KFS_ASSERT_TRUE(find_vma(&first, (unsigned long)first_addr) == NULL);
	KFS_ASSERT_TRUE(find_vma(&second, (unsigned long)second_addr) != NULL);
	KFS_ASSERT_EQ(0, do_munmap_mm(&second, (unsigned long)second_addr, PAGE_SIZE));

	free_page_tables((pgd_t *)first_pgd);
	free_page_tables((pgd_t *)second_pgd);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_returns_valid_address, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_inserts_vma, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_write_readable, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_munmap_removes_vma, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_zero_len_fails, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_no_anonymous_fails, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_multiple_non_overlapping, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_munmap_invalid_addr, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_mmap2_wrapper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sys_munmap_wrapper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_prot_read_only, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_munmap_remap_cycle, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_mm_isolates_vma_lists, setup_test, teardown_test),
};

int register_unit_tests_mmap(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
