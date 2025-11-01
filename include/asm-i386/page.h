#ifndef _ASM_I386_PAGE_H
#define _ASM_I386_PAGE_H

/* ページサイズ */
#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT) /* 4096バイト */
#define PAGE_MASK (~(PAGE_SIZE - 1))

/* ページアライメント */
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

/* ページフラグ */
#define _PAGE_PRESENT 0x001 /* ページが存在 */
#define _PAGE_RW 0x002		/* 読み書き可能 */
#define _PAGE_USER 0x004	/* ユーザーモード */

#endif /* _ASM_I386_PAGE_H */
