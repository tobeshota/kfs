#ifndef _ASM_I386_PAGE_H
#define _ASM_I386_PAGE_H

/* ページサイズ */
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT) /* 4096バイト */
#define PAGE_MASK (~(PAGE_SIZE - 1))  /* ページアラインされたアドレスを取り出すマスク */

/* ページアライメント */
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

/** カーネル空間のオフセット（Higher Half Kernel）
 * @details
 * カーネルは物理1MiBにロードされるが、仮想3GB以上で動作する．
 * よって，カーネル空間においては，物理アドレス == 仮想アドレス - PAGE_OFFSET である．
 *
 * @note カーネル空間のアドレス変換例
 * 物理アドレス          仮想アドレス
 * ─────────            ─────────
 * 0x00400000           0xC0400000
 *     ↑                    ↑
 *     │                    │
 *     │   +0xC0000000      │  ← 常に 3GB (PAGE_OFFSET) の差
 *     │   ─────────→       │
 *     │                    │
 * 0x00105000           0xC0105000  (例: カーネル関数)
 *     │                    │
 *     │   +0xC0000000      │
 *     │   ─────────→       │
 *     │                    │
 * 0x00100000           0xC0100000  (カーネル開始)
 */
#define PAGE_OFFSET 0xC0000000UL /* 3GB */

/** 仮想アドレスから物理アドレスに変換
 * @param x カーネル仮想アドレス (0xC0000000以上)
 * @return 物理アドレス
 * @note ページテーブルには物理アドレスを格納する必要がある
 * @note __pa()はカーネル空間専用．カーネルが配置された仮想アドレスと物理アドレスの変換を行う
 */
#define __pa(x) ((unsigned long)(x) - PAGE_OFFSET)

/** 物理アドレスから仮想アドレスに変換
 * @param x 物理アドレス
 * @return カーネル仮想アドレス (0xC0000000以上)
 * @note カーネルコードからメモリにアクセスする際は仮想アドレスを使用
 * @note __va()はカーネル空間専用．カーネルが配置された仮想アドレスと物理アドレスの変換を行う
 */
#define __va(x) ((void *)((unsigned long)(x) + PAGE_OFFSET))

/* ページフレーム番号変換 */
#define virt_to_pfn(kaddr) (__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_virt(pfn) __va((pfn) << PAGE_SHIFT)

#endif /* _ASM_I386_PAGE_H */
