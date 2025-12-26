#ifndef _ASM_I386_PGTABLE_H
#define _ASM_I386_PGTABLE_H

#include <asm-i386/page.h>
#include <kfs/stdint.h>

/* ========== ページテーブルエントリ定義 ========== */

/* ページディレクトリ/テーブルエントリのフラグ */
#define _PAGE_PRESENT 0x001	 /* P: ページが物理メモリに存在 */
#define _PAGE_RW 0x002		 /* R/W: 読み書き可能（1=書き込み可） */
#define _PAGE_USER 0x004	 /* U/S: ユーザーモードアクセス許可（1=許可） */
#define _PAGE_PWT 0x008		 /* PWT: Page Write-Through */
#define _PAGE_PCD 0x010		 /* PCD: Page Cache Disable */
#define _PAGE_ACCESSED 0x020 /* A: ページ/テーブルがアクセスされた */
#define _PAGE_DIRTY 0x040	 /* D: ページが書き込まれた（PTEのみ） */
#define _PAGE_PSE 0x080		 /* PS: ページサイズ（0=4KB, 1=4MB、PDEのみ） */
#define _PAGE_GLOBAL 0x100	 /* G: グローバルページ（TLBフラッシュ時も保持） */

/* ページディレクトリ/テーブルエントリの型 */
typedef uint32_t pte_t; /* ページテーブルエントリ */
typedef uint32_t pde_t; /* ページディレクトリエントリ */
typedef pde_t pgd_t; /* ページグローバルディレクトリ（i386ではページディレクトリと同じ） */

/** 1ページテーブルあたりのエントリ数
 * @note PoinTeRs PER Page Table Entry の略
 */
#define PTRS_PER_PTE 1024

/** 1ページディレクトリあたりのエントリ数
 * @note PoinTeRs PER Page Global Directory の略
 */
#define PTRS_PER_PGD 1024

/* 1ページテーブルがカバーするアドレス範囲（4MB） */
#define PGDIR_SIZE (1UL << 22) /* 4MB = 1024エントリ × 4KB/エントリ */
#define PGDIR_MASK (~(PGDIR_SIZE - 1))

/* ページテーブル操作マクロ */
#define pte_val(x) ((x))
#define pde_val(x) ((x))
#define __pte(x) ((pte_t){(x)})
#define __pde(x) ((pde_t){(x)})

/** ページディレクトリ/テーブル内のインデックス計算
 * @note pgd_indexの"pgd"は"page global directory"のこと．
 *       pgd_indexの由来はページディレクトリが多段化する64ビットISAと名称を合わせるため．
 *       i386ではページディレクトリが多段化しないため，実質的にはPDEのインデックスと同じ．
 */
#define pgd_index(address) (((address) >> 22) & 0x3ff)
#define pte_index(address) (((address) >> 12) & 0x3ff)

/* エントリから物理アドレスを取得 */
#define pte_page(pte) ((pte_val(pte) & PAGE_MASK))
#define pde_page(pde) ((pde_val(pde) & PAGE_MASK))

/* フラグ確認マクロ */
#define pte_present(pte) (pte_val(pte) & _PAGE_PRESENT)
#define pde_present(pde) (pde_val(pde) & _PAGE_PRESENT)

/* U/S: ユーザーモードアクセス許可の確認 */
#define pte_user(pte) (pte_val(pte) & _PAGE_USER)
#define pde_user(pde) (pde_val(pde) & _PAGE_USER)

/* R/W: 書き込み可能かの確認 */
#define pte_write(pte) (pte_val(pte) & _PAGE_RW)
#define pde_write(pde) (pde_val(pde) & _PAGE_RW)

/* A: アクセス済みかの確認 */
#define pte_accessed(pte) (pte_val(pte) & _PAGE_ACCESSED)
#define pde_accessed(pde) (pde_val(pde) & _PAGE_ACCESSED)

/* D: ダーティ（書き込み済み）かの確認（PTEのみ） */
#define pte_dirty(pte) (pte_val(pte) & _PAGE_DIRTY)

/* PS: ページサイズの確認（PDEのみ、0=4KB, 1=4MB） */
#define pde_large(pde) (pde_val(pde) & _PAGE_PSE)

/* ========== 標準フラグ組み合わせ（Linux 2.6.11互換） ========== */

/* カーネル用ページのデフォルトフラグ（R/W可、ユーザーアクセス不可） */
#define _PAGE_KERNEL (_PAGE_PRESENT | _PAGE_RW)

/* ユーザー用ページのデフォルトフラグ（R/W可、ユーザーアクセス可） */
#define _PAGE_USER_RW (_PAGE_PRESENT | _PAGE_RW | _PAGE_USER)

/* ユーザー用読み取り専用ページのフラグ */
#define _PAGE_USER_RO (_PAGE_PRESENT | _PAGE_USER)

/* ========== フラグ操作ヘルパー ========== */

/** PTEにユーザーモードアクセスを許可 */
static inline void pte_set_user(pte_t *pte)
{
	*pte |= _PAGE_USER;
}

/** PTEからユーザーモードアクセスを除去 */
static inline void pte_clear_user(pte_t *pte)
{
	*pte &= ~_PAGE_USER;
}

/** PTEを書き込み可能に設定 */
static inline void pte_set_write(pte_t *pte)
{
	*pte |= _PAGE_RW;
}

/** PTEを読み取り専用に設定 */
static inline void pte_clear_write(pte_t *pte)
{
	*pte &= ~_PAGE_RW;
}

/** PTEのアクセスフラグをクリア */
static inline void pte_clear_accessed(pte_t *pte)
{
	*pte &= ~_PAGE_ACCESSED;
}

/** PTEのダーティフラグをクリア */
static inline void pte_clear_dirty(pte_t *pte)
{
	*pte &= ~_PAGE_DIRTY;
}

/** PDEにユーザーモードアクセスを許可 */
static inline void pde_set_user(pde_t *pde)
{
	*pde |= _PAGE_USER;
}

/** PDEからユーザーモードアクセスを除去 */
static inline void pde_clear_user(pde_t *pde)
{
	*pde &= ~_PAGE_USER;
}

/** PDEのアクセスフラグをクリア */
static inline void pde_clear_accessed(pde_t *pde)
{
	*pde &= ~_PAGE_ACCESSED;
}

/* ========== CR3操作 ========== */

/** CR3レジスタにページディレクトリを書き込む
 * @remark CR3レジスタはページディレクトリのベースアドレスを格納することから
 *         PDBR[page directory base register]とも呼ばれる
 */
static inline void load_cr3(unsigned long pgdir)
{
	__asm__ __volatile__("movl %0, %%cr3" ::"r"(pgdir) : "memory");
}

/* CR3レジスタを読み取る */
static inline unsigned long read_cr3(void)
{
	unsigned long cr3;
	__asm__ __volatile__("movl %%cr3, %0" : "=r"(cr3));
	return cr3;
}

/* ========== ページング有効化 ========== */

/** CR0のPGビットを設定してページングを有効化する
 * @brief CR0のPGビットが1のとき，ページングが有効になりCR3レジスタが使用されるようになる
 * @see Section 4.1.3(Control Registers) of
 *      https://pdos.csail.mit.edu/6.828/2018/readings/i386.pdf
 */
static inline void enable_paging(void)
{
	unsigned long cr0;
	__asm__ __volatile__("movl %%cr0, %0\n"
						 "orl $0x80000000, %0\n" /* PGビット(bit 31)を設定 */
						 "movl %0, %%cr0\n"
						 : "=r"(cr0)
						 :
						 : "memory");
}

/** TLBをフラッシュする
 * @brief    CPUはCR3に書き込みが行われると，現在のTLB[Translation Lookaside
 *           Buffer]の値を古いものとみなし，使わないようにする．そこで，
 *           CR3レジスタの値をtmpregに書き，それを再度CR3レジスタに書き込むことで
 *           CPUに今のTLBを無効化して再ロードさせる．
 * @example  PTEを書き換えた後は__flush_tlb()を実行してCPUのTLBを無効化する必要がある．
 *           その理由は，新しいマッピングがCPUに反映されず予期せぬアクセスが起こる可能性があるため．
 */
static inline void __flush_tlb(void)
{
	unsigned long tmpreg;
	__asm__ __volatile__("movl %%cr3, %0\n"
						 "movl %0, %%cr3\n"
						 : "=r"(tmpreg)
						 :
						 : "memory");
}

/** PDEに物理ページテーブルのアドレスとフラグを組み合わせて書き込む
 * @param pde 書き込み先のPDEポインタ
 * @param addr ページテーブルの物理アドレス
 * @param flags PDE用のフラグビットマスク
 */
static inline void set_pde(pde_t *pde, unsigned long addr, unsigned long flags)
{
	*pde = (addr & PAGE_MASK) | (flags & ~PAGE_MASK);
}

/** PTEに物理ページのアドレスとフラグを組み合わせて書き込む
 * @param pte 書き込み先のPTEポインタ
 * @param addr 物理ページのアドレス
 * @param flags ページフラグのビットマスク
 * @note ページのサイズは4KB固定(定義元：PAGE_SIZE)
 * @example
 * // 物理アドレス0x00100000からの4KBを読み書き可能のページとする
 * set_pte(&page_table[pte_index], 0x00100000, _PAGE_PRESENT | _PAGE_RW);
 *
 */
static inline void set_pte(pte_t *pte, unsigned long addr, unsigned long flags)
{
	*pte = (addr & PAGE_MASK) | (flags & ~PAGE_MASK);
}

/* ページテーブルエントリをクリア */
static inline void pte_clear(pte_t *pte)
{
	*pte = 0;
}

/* ページディレクトリエントリをクリア */
static inline void pde_clear(pde_t *pde)
{
	*pde = 0;
}

pte_t *get_pte(unsigned long vaddr);
int map_page(unsigned long vaddr, unsigned long paddr, unsigned long flags);

#endif /* _ASM_I386_PGTABLE_H */
