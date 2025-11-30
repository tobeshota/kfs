/** 仮想メモリアロケータ（vmalloc/vfree）
 * - Linux 2.6.11のmm/vmalloc.cに相当
 * - 物理的に非連続なメモリを仮想的に連続なメモリとして割り当てる
 */

#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/printk.h>
#include <kfs/slab.h>
#include <kfs/stddef.h>
#include <kfs/vmalloc.h>

/* vmalloc領域の管理構造体（Linux 2.6.11のvm_structに相当） */
struct vm_struct
{
	void *addr;				/* 仮想アドレス */
	unsigned long size;		/* 割り当てサイズ（バイト単位） */
	struct vm_struct *next; /* 次のvm_struct（リンクリスト） */
};

/* vmalloc領域のリスト */
static struct vm_struct *vmlist = NULL;

/* vbrk用の仮想メモリヒープ境界 */
static void *vheap_start = NULL;
static void *vheap_end = NULL;
static void *vheap_brk = NULL;

/** vmalloc領域を初期化する
 * @details これにより，vmalloc/vfreeが使用可能になる
 * @note Linux 2.6.11のvmalloc_init()に相当する
 */
void vmalloc_init(void)
{
	/* 初期化時はリストを空にする */
	vmlist = NULL;

	/* vbrk用のヒープ境界を初期化 */
	vheap_start = NULL;
	vheap_end = NULL;
	vheap_brk = NULL;

	printk(KERN_INFO "vmalloc initialized\n");
}

/** 指定したサイズの仮想メモリを割り当てる
 * @param size 割り当てサイズ（バイト単位）
 * @return 割り当てた仮想アドレス、失敗時はNULL
 * @note Linux 2.6.11のvmalloc()に相当する
 */
void *vmalloc(unsigned long size)
{
	struct vm_struct *vm;
	struct vm_area_struct *vma;
	unsigned long addr;
	unsigned long aligned_size;
	unsigned long nr_pages;
	unsigned long i;

	if (size == 0)
	{
		return NULL;
	}

	/** sizeをPAGE_SIZEの倍数に切り上げて揃える
	 * @details
	 * - size + PAGE_SIZE - 1 上方向へ丸めるための調整
	 * - ~(PAGE_SIZE - 1) 下位ビットをクリアしてページ境界に揃える
	 */
	aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* ページ数を計算 */
	nr_pages = aligned_size >> PAGE_SHIFT;

	/* 未使用の仮想アドレス領域を探す */
	addr = get_unmapped_area(aligned_size);
	if (addr == 0)
	{
		printk(KERN_WARNING "vmalloc: no space for %lu bytes\n", size);
		return NULL;
	}

	/* vm_struct（vmalloc管理構造体）を割り当て */
	vm = (struct vm_struct *)kmalloc(sizeof(struct vm_struct));
	if (vm == NULL)
	{
		printk(KERN_WARNING "vmalloc: failed to allocate vm_struct\n");
		return NULL;
	}

	/* vm_area_struct（VMA）を割り当て */
	vma = (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
	if (vma == NULL)
	{
		kfree(vm);
		printk(KERN_WARNING "vmalloc: failed to allocate vm_area_struct\n");
		return NULL;
	}

	/* VMAを初期化 */
	vma->vm_start = addr;
	vma->vm_end = addr + aligned_size;
	vma->vm_flags = VM_READ | VM_WRITE; /* 読み書き可能 */
	vma->vm_next = NULL;

	/* VMAをリストに挿入 */
	if (insert_vm_area(vma) != 0)
	{
		kfree(vma);
		kfree(vm);
		printk(KERN_WARNING "vmalloc: failed to insert vm_area\n");
		return NULL;
	}

	/* 物理ページを割り当ててマッピング */
	for (i = 0; i < nr_pages; i++)
	{
		struct page *page;
		unsigned long vaddr = addr + (i << PAGE_SHIFT);
		unsigned long paddr;

		/* 物理ページを1ページずつ割り当て */
		page = alloc_pages(GFP_KERNEL, 0);
		if (page == NULL)
		{
			/* 失敗した場合は既に割り当てたページを解放 */
			unsigned long j;
			for (j = 0; j < i; j++)
			{
				unsigned long free_vaddr = addr + (j << PAGE_SHIFT);
				struct page *free_page = (struct page *)virt_to_phys((void *)free_vaddr);
				free_pages(free_page, 0);
			}
			remove_vm_area(addr);
			kfree(vma);
			kfree(vm);
			printk(KERN_WARNING "vmalloc: failed to allocate page %lu/%lu\n", i, nr_pages);
			return NULL;
		}

		/* 物理ページのアドレスを取得 */
		paddr = (unsigned long)page;

		/* 仮想アドレスと物理アドレスをページテーブルでマッピング（カーネル専用） */
		if (map_page_vmalloc(vaddr, paddr, _PAGE_KERNEL) != 0)
		{
			/* マッピング失敗時は物理ページも含めて解放 */
			unsigned long j;
			free_pages(page, 0);
			for (j = 0; j < i; j++)
			{
				unsigned long free_vaddr = addr + (j << PAGE_SHIFT);
				struct page *free_page = (struct page *)virt_to_phys((void *)free_vaddr);
				free_pages(free_page, 0);
			}
			remove_vm_area(addr);
			kfree(vma);
			kfree(vm);
			printk(KERN_WARNING "vmalloc: failed to map page %lu/%lu\n", i, nr_pages);
			return NULL;
		}
	}

	/* vm_structを初期化してリストに追加 */
	vm->addr = (void *)addr;
	vm->size = aligned_size;
	vm->next = vmlist;
	vmlist = vm;

	printk(KERN_INFO "vmalloc: allocated %lu bytes at 0x%lx\n", size, addr);
	return (void *)addr;
}

/** vmalloc()で割り当てた仮想メモリを解放する
 * @param addr 解放する仮想アドレス
 * @note Linux 2.6.11のvfree()に相当する
 */
void vfree(void *addr)
{
	struct vm_struct *vm, *prev;
	unsigned long vaddr = (unsigned long)addr;
	unsigned long nr_pages;
	unsigned long i;

	if (addr == NULL)
	{
		return;
	}

	/* vmlistから該当するvm_structを探す */
	prev = NULL;
	for (vm = vmlist; vm != NULL; vm = vm->next)
	{
		if (vm->addr == addr)
		{
			break;
		}
		prev = vm;
	}

	if (vm == NULL)
	{
		printk(KERN_WARNING "vfree: address 0x%lx not found\n", vaddr);
		return;
	}

	/* 物理ページを解放 */
	nr_pages = vm->size >> PAGE_SHIFT;
	for (i = 0; i < nr_pages; i++)
	{
		unsigned long free_vaddr = vaddr + (i << PAGE_SHIFT);
		struct page *page = (struct page *)virt_to_phys((void *)free_vaddr);
		free_pages(page, 0);
	}

	/* VMAをリストから削除 */
	remove_vm_area(vaddr);

	/* vmlistから削除 */
	if (prev == NULL)
	{
		vmlist = vm->next;
	}
	else
	{
		prev->next = vm->next;
	}

	/* vm_structを解放 */
	kfree(vm);

	printk(KERN_INFO "vfree: freed %lu bytes at 0x%lx\n", vm->size, vaddr);
}

/** 割り当て済み仮想メモリのサイズを取得する
 * @param addr 仮想アドレス
 * @return サイズ（バイト単位）、見つからない場合は0
 */
size_t vsize(void *addr)
{
	struct vm_struct *vm;

	if (addr == NULL)
	{
		return 0;
	}

	/* vmlistから該当するvm_structを探す */
	for (vm = vmlist; vm != NULL; vm = vm->next)
	{
		if (vm->addr == addr)
		{
			return vm->size;
		}
	}

	return 0;
}

/** 仮想メモリヒープの拡張する
 * @param increment 増減サイズ（バイト単位）
 * @return 新しいヒープ境界、失敗時はNULL
 * @note Linux 2.6.11にはない独自関数（kbrk()の仮想メモリ版）
 * @details
 * - increment > 0: ヒープを拡張
 * - increment < 0: ヒープを縮小
 * - increment == 0: 現在のヒープ境界を返す
 */
void *vbrk(long increment)
{
	void *new_brk;
	unsigned long expand_size;
	void *new_region;

	/* 初回呼び出し時：初期ヒープ領域を確保 */
	if (vheap_start == NULL)
	{
		/* 初期サイズ：1MB */
		vheap_start = vmalloc(1024 * 1024);
		if (vheap_start == NULL)
		{
			printk(KERN_WARNING "vbrk: failed to initialize heap\n");
			return NULL;
		}
		vheap_end = (void *)((unsigned long)vheap_start + 1024 * 1024);
		vheap_brk = vheap_start;
		printk(KERN_INFO "vbrk: initialized heap at 0x%lx-0x%lx\n", (unsigned long)vheap_start,
			   (unsigned long)vheap_end);
	}

	/* increment == 0: 現在のヒープ境界を返す */
	if (increment == 0)
	{
		return vheap_brk;
	}

	new_brk = (void *)((long)vheap_brk + increment);

	/* ヒープ縮小の場合 */
	if (increment < 0)
	{
		/* ヒープ開始位置より前には戻せない */
		if (new_brk < vheap_start)
		{
			printk(KERN_WARNING "vbrk: cannot shrink below heap start\n");
			return NULL;
		}
		vheap_brk = new_brk;
		printk(KERN_INFO "vbrk: shrunk by %ld bytes to 0x%lx\n", -increment, (unsigned long)new_brk);
		return new_brk;
	}

	/* ヒープ拡張の場合 */
	/* 現在の割り当て済み領域内に収まる場合 */
	if (new_brk <= vheap_end)
	{
		vheap_brk = new_brk;
		printk(KERN_INFO "vbrk: expanded by %ld bytes to 0x%lx\n", increment, (unsigned long)new_brk);
		return new_brk;
	}

	/* 現在の領域を超える場合：新しい領域を追加で確保 */
	expand_size = (unsigned long)new_brk - (unsigned long)vheap_end;
	/* ページサイズに切り上げ */
	expand_size = (expand_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	new_region = vmalloc(expand_size);
	if (new_region == NULL)
	{
		printk(KERN_WARNING "vbrk: failed to expand heap by %lu bytes\n", expand_size);
		return NULL;
	}

	printk(KERN_INFO "vbrk: allocated additional region at 0x%lx (%lu bytes)\n", (unsigned long)new_region,
		   expand_size);

	/* ヒープ終端を更新（注意：追加領域は連続していない可能性がある） */
	vheap_end = (void *)((unsigned long)vheap_end + expand_size);
	vheap_brk = new_brk;

	printk(KERN_INFO "vbrk: expanded by %ld bytes to 0x%lx\n", increment, (unsigned long)new_brk);
	return new_brk;
}
