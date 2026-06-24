#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/mm_types.h>
#include <kfs/mman.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/stddef.h>

/** mmap失敗時に確保済みページを解放する
 * @param mm ロールバック対象のメモリディスクリプタ
 * @param start 開始仮想アドレス
 * @param nr_pages 解除するページ数
 */
static void rollback_mmap_pages(struct mm_struct *mm, unsigned long start, unsigned long nr_pages)
{
	unsigned long i;

	if (mm == NULL || mm->pgd == NULL)
	{
		return;
	}

	for (i = 0; i < nr_pages; i++)
	{
		unsigned long va = start + (i << PAGE_SHIFT);
		pte_t *pte = get_pte(mm->pgd, va);
		if (pte != NULL && pte_present(*pte))
		{
			free_pages((struct page *)pte_page(*pte), 0);
			unmap_page(mm->pgd, va);
		}
	}
}

/** 指定mm_structへ匿名メモリマッピングを確保する
 * @param mm マッピング先のメモリディスクリプタ
 * @param addr 希望アドレス（NULLで自動選択）
 * @param len 確保サイズ
 * @param prot PROT_NONE，PROT_READ，PROT_WRITE，PROT_EXECの組み合わせ
 * @param flags MAP_ANONYMOUS必須
 * @return 確保したユーザ仮想アドレス，失敗時MAP_FAILED
 */
void *do_mmap_mm(struct mm_struct *mm, void *addr, unsigned long len, int prot, int flags)
{
	unsigned long vaddr;
	unsigned long aligned_len;
	unsigned long nr_pages;
	unsigned long i;
	unsigned long page_flags;
	struct vm_area_struct *vma;

	(void)addr;

	if (mm == NULL || mm->pgd == NULL)
	{
		return MAP_FAILED;
	}

	if (!(flags & MAP_ANONYMOUS))
	{
		printk(KERN_WARNING "do_mmap: only MAP_ANONYMOUS is supported\n");
		return MAP_FAILED;
	}

	if (len == 0)
	{
		return MAP_FAILED;
	}

	aligned_len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	vaddr = get_unmapped_area_user(mm, aligned_len);
	if (vaddr == 0)
	{
		printk(KERN_WARNING "do_mmap: no user unmapped area for %lu bytes\n", len);
		return MAP_FAILED;
	}

	page_flags = _PAGE_PRESENT | _PAGE_USER;
	if (prot & PROT_WRITE)
	{
		page_flags |= _PAGE_RW;
	}

	nr_pages = aligned_len >> PAGE_SHIFT;
	for (i = 0; i < nr_pages; i++)
	{
		struct page *page;
		unsigned long cur_vaddr = vaddr + (i << PAGE_SHIFT);
		unsigned long paddr;

		page = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
		if (page == NULL)
		{
			rollback_mmap_pages(mm, vaddr, i);
			printk(KERN_WARNING "do_mmap: alloc_pages failed at page %lu/%lu\n", i, nr_pages);
			return MAP_FAILED;
		}

		paddr = (unsigned long)page;
		if (map_page(mm->pgd, cur_vaddr, paddr, page_flags) != 0)
		{
			free_pages(page, 0);
			rollback_mmap_pages(mm, vaddr, i);
			printk(KERN_WARNING "do_mmap: map_page failed at page %lu/%lu\n", i, nr_pages);
			return MAP_FAILED;
		}
	}

	vma = (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
	if (vma == NULL)
	{
		rollback_mmap_pages(mm, vaddr, nr_pages);
		printk(KERN_WARNING "do_mmap: kmalloc for vma failed\n");
		return MAP_FAILED;
	}

	vma->vm_start = vaddr;
	vma->vm_end = vaddr + aligned_len;
	vma->vm_flags = (unsigned long)prot;
	vma->vm_next = NULL;

	if (insert_vm_area(mm, vma) != 0)
	{
		rollback_mmap_pages(mm, vaddr, nr_pages);
		kfree(vma);
		printk(KERN_WARNING "do_mmap: insert_vm_area failed at 0x%lx\n", vaddr);
		return MAP_FAILED;
	}

	printk(KERN_INFO "do_mmap: mapped %lu bytes at 0x%lx (prot=0x%x, user)\n", aligned_len, vaddr, prot);
	return (void *)vaddr;
}

/** currentのmm_structへ匿名メモリマッピングを確保する
 * @param addr 希望アドレス（NULLで自動選択）
 * @param len 確保サイズ
 * @param prot PROT_NONE，PROT_READ，PROT_WRITE，PROT_EXECの組み合わせ
 * @param flags MAP_ANONYMOUS必須
 * @return 確保したユーザ仮想アドレス，失敗時MAP_FAILED
 */
void *do_mmap(void *addr, unsigned long len, int prot, int flags)
{
	if (current == NULL)
	{
		return MAP_FAILED;
	}
	return do_mmap_mm(current->mm, addr, len, prot, flags);
}

/** 指定mm_structのメモリマッピングを解放する
 * @param mm 解放対象のメモリディスクリプタ
 * @param addr 解放する領域の開始アドレス
 * @param len 解放サイズ（現実装ではVMA全体を解放）
 * @return 成功時0，失敗時-1
 */
int do_munmap_mm(struct mm_struct *mm, unsigned long addr, unsigned long len)
{
	struct vm_area_struct *vma;
	unsigned long nr_pages;
	unsigned long i;
	unsigned long vma_size;

	(void)len;

	if (mm == NULL || mm->pgd == NULL)
	{
		return -1;
	}

	vma = find_vma(mm, addr);
	if (vma == NULL)
	{
		printk(KERN_WARNING "do_munmap: no VMA found at 0x%lx\n", addr);
		return -1;
	}

	vma_size = vma->vm_end - vma->vm_start;
	nr_pages = vma_size >> PAGE_SHIFT;
	for (i = 0; i < nr_pages; i++)
	{
		unsigned long va = vma->vm_start + (i << PAGE_SHIFT);
		pte_t *pte = get_pte(mm->pgd, va);
		if (pte != NULL && pte_present(*pte))
		{
			free_pages((struct page *)pte_page(*pte), 0);
			unmap_page(mm->pgd, va);
		}
	}

	vma = remove_vm_area(mm, vma->vm_start);
	if (vma != NULL)
	{
		kfree(vma);
	}

	printk(KERN_INFO "do_munmap: unmapped %lu bytes at 0x%lx\n", vma_size, addr);
	return 0;
}

/** currentのmm_structからメモリマッピングを解放する
 * @param addr 解放する領域の開始アドレス
 * @param len 解放サイズ（現実装ではVMA全体を解放）
 * @return 成功時0，失敗時-1
 */
int do_munmap(unsigned long addr, unsigned long len)
{
	if (current == NULL)
	{
		return -1;
	}
	return do_munmap_mm(current->mm, addr, len);
}

/** mmap2システムコール用ヘルパー
 * @param addr 希望アドレス
 * @param len 確保サイズ
 * @param prot 保護フラグ
 * @param flags mmapフラグ
 * @param fd 未使用
 * @param pgoff 未使用
 * @return 確保したユーザ仮想アドレス，失敗時MAP_FAILED
 */
void *sys_mmap2(unsigned long addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff)
{
	(void)fd;
	(void)pgoff;
	return do_mmap((void *)addr, len, prot, flags);
}

/** munmapシステムコール用ヘルパー
 * @param addr 解放する領域の開始アドレス
 * @param len 解放サイズ
 * @return 成功時0，失敗時-1
 */
int sys_munmap(unsigned long addr, unsigned long len)
{
	return do_munmap(addr, len);
}
