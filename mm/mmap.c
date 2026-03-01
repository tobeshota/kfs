#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/mman.h>
#include <kfs/printk.h>
#include <kfs/slab.h>
#include <kfs/stddef.h>

/** 匿名メモリマッピングを確保する（ユーザ空間アドレスを返す）
 * @param addr  希望アドレス（NULL で自動選択）
 * @param len   確保サイズ（vmalloc 内部で PAGE_SIZE に切り上げ）
 * @param prot  PROT_NONE, PROT_READ, PROT_WRITE, PROT_EXEC の組み合わせ
 * @param flags MAP_ANONYMOUS 必須（ファイルマッピングは未サポート）
 * @return 確保したユーザ仮想アドレス（< PAGE_OFFSET）、失敗時は MAP_FAILED
 *
 * 処理フロー:
 *   get_unmapped_area_user() → alloc_pages() × nr_pages
 *   → map_page_vmalloc(_PAGE_USER) → insert_vm_area()
 */
void *do_mmap(void *addr, unsigned long len, int prot, int flags)
{
	unsigned long vaddr;
	unsigned long aligned_len;
	unsigned long nr_pages;
	unsigned long i;
	unsigned long page_flags;
	struct vm_area_struct *vma;

	/* 希望アドレス指定は未サポート（自動選択のみ） */
	(void)addr;

	/* 現時点では MAP_ANONYMOUS のみサポート */
	if (!(flags & MAP_ANONYMOUS))
	{
		printk(KERN_WARNING "do_mmap: only MAP_ANONYMOUS is supported\n");
		return MAP_FAILED;
	}

	if (len == 0)
	{
		return MAP_FAILED;
	}

	/* len を PAGE_SIZE の倍数に切り上げ */
	aligned_len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* ユーザ空間の未使用仮想アドレスを取得 */
	vaddr = get_unmapped_area_user(aligned_len);
	if (vaddr == 0)
	{
		printk(KERN_WARNING "do_mmap: no user unmapped area for %lu bytes\n", len);
		return MAP_FAILED;
	}

	/* prot → ページテーブルフラグ変換（_PAGE_USER でユーザモードアクセスを許可） */
	page_flags = _PAGE_PRESENT | _PAGE_USER;
	if (prot & PROT_WRITE)
	{
		page_flags |= _PAGE_RW;
	}

	/* 物理ページを確保してユーザ空間にマッピング */
	nr_pages = aligned_len >> PAGE_SHIFT;
	for (i = 0; i < nr_pages; i++)
	{
		struct page *page;
		unsigned long cur_vaddr = vaddr + (i << PAGE_SHIFT);
		unsigned long paddr;

		page = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
		if (page == NULL)
		{
			/* 失敗: マップ済みページを PTE 経由で解放してロールバック */
			unsigned long j;
			for (j = 0; j < i; j++)
			{
				unsigned long rv = vaddr + (j << PAGE_SHIFT);
				pte_t *pte = get_pte(rv);
				if (pte && pte_present(*pte))
				{
					free_pages((struct page *)pte_page(*pte), 0);
				}
			}
			printk(KERN_WARNING "do_mmap: alloc_pages failed at page %lu/%lu\n", i, nr_pages);
			return MAP_FAILED;
		}

		paddr = (unsigned long)page;

		if (map_page_vmalloc(cur_vaddr, paddr, page_flags) != 0)
		{
			/* マッピング失敗: このページ含め確保済みを解放 */
			unsigned long j;
			free_pages(page, 0);
			for (j = 0; j < i; j++)
			{
				unsigned long rv = vaddr + (j << PAGE_SHIFT);
				pte_t *pte = get_pte(rv);
				if (pte && pte_present(*pte))
				{
					free_pages((struct page *)pte_page(*pte), 0);
				}
			}
			printk(KERN_WARNING "do_mmap: map_page_vmalloc failed at page %lu/%lu\n", i, nr_pages);
			return MAP_FAILED;
		}
	}

	/* VMA 登録（カーネル vma_list と共有: アドレスで区別可能） */
	vma = (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
	if (vma == NULL)
	{
		unsigned long j;
		for (j = 0; j < nr_pages; j++)
		{
			unsigned long rv = vaddr + (j << PAGE_SHIFT);
			pte_t *pte = get_pte(rv);
			if (pte && pte_present(*pte))
			{
				free_pages((struct page *)pte_page(*pte), 0);
			}
		}
		printk(KERN_WARNING "do_mmap: kmalloc for vma failed\n");
		return MAP_FAILED;
	}

	vma->vm_start = vaddr;
	vma->vm_end = vaddr + aligned_len;
	vma->vm_flags = (unsigned long)prot;
	vma->vm_next = NULL;

	if (insert_vm_area(vma) != 0)
	{
		unsigned long j;
		for (j = 0; j < nr_pages; j++)
		{
			unsigned long rv = vaddr + (j << PAGE_SHIFT);
			pte_t *pte = get_pte(rv);
			if (pte && pte_present(*pte))
			{
				free_pages((struct page *)pte_page(*pte), 0);
			}
		}
		kfree(vma);
		printk(KERN_WARNING "do_mmap: insert_vm_area failed at 0x%lx\n", vaddr);
		return MAP_FAILED;
	}

	printk(KERN_INFO "do_mmap: mapped %lu bytes at 0x%lx (prot=0x%x, user)\n", aligned_len, vaddr, prot);
	return (void *)vaddr;
}

/** メモリマッピングを解放する
 * @param addr  解放する領域の開始アドレス
 * @param len   解放サイズ（現実装では VMA 全体を解放）
 * @return 成功時 0、失敗時 -1
 *
 * @note ページテーブルエントリのクリア（TLB フラッシュ）は未実装のため、
 *       解放後もページテーブル上には古いエントリが残る。
 */
int do_munmap(unsigned long addr, unsigned long len)
{
	struct vm_area_struct *vma;
	unsigned long nr_pages;
	unsigned long i;
	unsigned long vma_size;

	(void)len; /* 現実装では VMA 全体を一括解放 */

	vma = find_vma(addr);
	if (vma == NULL)
	{
		printk(KERN_WARNING "do_munmap: no VMA found at 0x%lx\n", addr);
		return -1;
	}

	/*
	 * ユーザ空間は線形マッピング外のため virt_to_phys() は使えない。
	 * PTE を逆引きして物理アドレスを取得してから解放する。
	 */
	vma_size = vma->vm_end - vma->vm_start;
	nr_pages = vma_size >> PAGE_SHIFT;
	for (i = 0; i < nr_pages; i++)
	{
		unsigned long va = vma->vm_start + (i << PAGE_SHIFT);
		pte_t *pte = get_pte(va);
		if (pte && pte_present(*pte))
		{
			free_pages((struct page *)pte_page(*pte), 0);
		}
	}

	/* VMA をリストから削除して構造体を解放 */
	remove_vm_area(addr);
	kfree(vma);

	printk(KERN_INFO "do_munmap: unmapped %lu bytes at 0x%lx\n", vma_size, addr);
	return 0;
}

/** mmap2 システムコール用ヘルパー
 * @note fd / pgoff は現実装では無視（MAP_ANONYMOUS のみサポート）
 */
void *sys_mmap2(unsigned long addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff)
{
	(void)fd;
	(void)pgoff;
	return do_mmap((void *)addr, len, prot, flags);
}

/* munmap システムコール用ヘルパー */
int sys_munmap(unsigned long addr, unsigned long len)
{
	return do_munmap(addr, len);
}
