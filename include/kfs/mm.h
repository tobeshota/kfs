#ifndef _KFS_MM_H
#define _KFS_MM_H

#include <kfs/stddef.h>
#include <kfs/stdint.h>

/* 物理アドレスと仮想アドレスの変換マクロ */
/* PAGE_OFFSETはasm-i386/page.hで定義済み */
#include <asm-i386/page.h>

#define virt_to_phys(addr) __pa(addr)
#define phys_to_virt(addr) __va(addr)

/* ページフレーム番号 (PFN) 操作 */
typedef unsigned long pfn_t;

#define page_to_pfn(page) ((unsigned long)(page) >> 12)
#define pfn_to_page(pfn) ((void *)((pfn) << 12))

/* struct page型定義（Linux 2.6.11互換、簡略版） */
struct page;

/* 仮想メモリ領域フラグ (Linux 2.6.11のvm_flagsに相当) */
#define VM_READ 0x00000001	/* 読み取り可能 */
#define VM_WRITE 0x00000002 /* 書き込み可能 */
#define VM_EXEC 0x00000004	/* 実行可能 */

/* メモリリージョンのリストの先頭アドレス */
struct vm_area_struct
{
	unsigned long vm_start;			/* 開始仮想アドレス */
	unsigned long vm_end;			/* 終了仮想アドレス（排他的） */
	unsigned long vm_flags;			/* アクセス権限フラグ */
	struct vm_area_struct *vm_next; /* 次のVMA（リンクリスト） */
};

/* 物理ページ管理関数 (mm/page_alloc.c) */
struct page *alloc_pages(unsigned int gfp_mask, unsigned int order);
void free_pages(struct page *page, unsigned int order);

/* 仮想メモリ管理関数 (mm/memory.c) */
struct vm_area_struct *find_vma(unsigned long addr);
int insert_vm_area(struct vm_area_struct *vma);
void remove_vm_area(unsigned long addr);
unsigned long get_unmapped_area(size_t len);

/* ページング関連関数 */
void paging_init(void);

/* メモリ初期化関数 */
void mem_init(void);

/* テスト用リセット関数 */
void page_allocator_reset_for_test(void);
void vm_reset_for_test(void);

#endif /* _KFS_MM_H */
