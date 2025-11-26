#ifndef _ASM_I386_PAGE_H
#define _ASM_I386_PAGE_H

/* ページサイズ */
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT) /* 4096バイト */
#define PAGE_MASK (~(PAGE_SIZE - 1))  /* ページアラインされたアドレスを取り出すマスク */

/* ページアライメント */
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

/* カーネル空間のオフセット */
#define PAGE_OFFSET 0xC0000000UL /* 3GB */

/* 物理アドレス ⟷ 仮想アドレス変換（恒等マッピング + PAGE_OFFSET） */
#define __pa(x) ((unsigned long)(x) - PAGE_OFFSET)
#define __va(x) ((void *)((unsigned long)(x) + PAGE_OFFSET))

/* 現在は恒等マッピングなので変換不要だが、将来の拡張のため定義 */
#ifndef CONFIG_PAGING_ENABLED
#undef __pa
#undef __va
#define __pa(x) ((unsigned long)(x))
#define __va(x) ((void *)(x))
#endif

/* ページフレーム番号変換 */
#define virt_to_pfn(kaddr) (__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_virt(pfn) __va((pfn) << PAGE_SHIFT)

#endif /* _ASM_I386_PAGE_H */
