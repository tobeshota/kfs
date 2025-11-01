#ifndef _KFS_MM_H
#define _KFS_MM_H

/**
 * Memory Management Core Definitions - KFS-3 Minimal Implementation
 * Based on Linux 2.6.11 include/linux/mm.h (simplified for KFS-3)
 *
 * KFS-3必須: ページング、カーネル空間/ユーザー空間定義
 */

#include <kfs/stddef.h>
#include <kfs/stdint.h>

/* カーネル空間とユーザー空間の境界（KFS-3必須） */
#define PAGE_OFFSET 0xC0000000 /* 3GB: カーネル空間とユーザー空間の境界 */

/* 物理アドレスと仮想アドレスの変換マクロ */
/* KFS-3では恒等マッピングを使用（virt == phys） */
#define virt_to_phys(addr) ((unsigned long)(addr))
#define phys_to_virt(addr) ((void *)(addr))

/* ページフレーム番号 (PFN) 操作 */
typedef unsigned long pfn_t;

#define page_to_pfn(page) ((unsigned long)(page) >> 12)
#define pfn_to_page(pfn) ((void *)((pfn) << 12))

/* メモリ初期化関数（KFS-3必須） */
void mem_init(void);

#endif /* _KFS_MM_H */
