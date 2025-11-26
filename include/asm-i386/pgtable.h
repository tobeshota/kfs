#ifndef _ASM_I386_PGTABLE_H
#define _ASM_I386_PGTABLE_H

#include <asm-i386/page.h>
#include <kfs/stdint.h>

/* ========== ページテーブルエントリ定義 ========== */

/* ページディレクトリ/テーブルエントリのフラグ */
#define _PAGE_PRESENT 0x001 /* ページが存在 */
#define _PAGE_RW 0x002		/* 読み書き可能 */
#define _PAGE_USER 0x004	/* ユーザーモード */

/* ページディレクトリ/テーブルエントリの型 */
typedef uint32_t pte_t; /* ページテーブルエントリ */
typedef uint32_t pde_t; /* ページディレクトリエントリ */

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

void paging_init(void);
pte_t *get_pte(unsigned long vaddr);
int map_page(unsigned long vaddr, unsigned long paddr, unsigned long flags);

#endif /* _ASM_I386_PGTABLE_H */
