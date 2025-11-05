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

/* ページング関連関数 */
void paging_init(void);

/* メモリ初期化関数 */
void mem_init(void);

#endif /* _KFS_MM_H */
