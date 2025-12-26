#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/errno.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/string.h>

/* External page directory set up by boot.S */
extern pde_t boot_page_directory[];

/** 仮想アドレスに対応するPTEを取得する
 * @param vaddr 仮想アドレス
 * @return PTEへのポインタ、エラー時NULL
 */
pte_t *get_pte(unsigned long vaddr)
{
	int pde_idx, pte_idx;
	pde_t *pde;
	pte_t *pte_table;
	unsigned long pte_table_phys;

	pde_idx = pgd_index(vaddr);
	pde = &boot_page_directory[pde_idx];

	/* ページディレクトリエントリが存在するかチェック */
	if (!pde_present(*pde))
	{
		return NULL;
	}

	/* ページテーブルを取得（物理アドレス→仮想アドレス変換） */
	pte_table_phys = pde_page(*pde);
	pte_table = (pte_t *)__va(pte_table_phys);
	pte_idx = pte_index(vaddr);

	return &pte_table[pte_idx];
}

/** 仮想アドレスから対応するページテーブルを取得または作成する
 * @param vaddr 仮想アドレス
 * @return ページテーブルへのポインタ、エラー時NULL
 */
static pte_t *get_or_create_page_table(unsigned long vaddr)
{
	int pde_idx;
	pde_t *pde;
	pte_t *pte_table;
	struct page *page;
	unsigned long pte_table_phys;

	pde_idx = pgd_index(vaddr);
	pde = &boot_page_directory[pde_idx];

	/* ページテーブルが既に存在する場合 */
	if (pde_present(*pde))
	{
		pte_table_phys = pde_page(*pde);
		return (pte_t *)__va(pte_table_phys);
	}

	/* 新しいページテーブルを割り当て */
	page = alloc_pages(GFP_KERNEL, 0);
	if (page == NULL)
	{
		printk(KERN_WARNING "Failed to allocate page table\n");
		return NULL;
	}

	pte_table = (pte_t *)page;

	/* ページテーブルを初期化（全エントリをクリア） */
	memset(pte_table, 0, PAGE_SIZE);

	/* ページディレクトリエントリを設定（カーネル用、物理アドレスを使用） */
	pte_table_phys = __pa((unsigned long)pte_table);
	set_pde(pde, pte_table_phys, _PAGE_KERNEL);

	return pte_table;
}

/** 仮想アドレスを物理アドレスにマップ
 * @param vaddr 仮想アドレス（4KBアライメント）
 * @param paddr 物理アドレス（4KBアライメント）
 * @param flags ページフラグ
 * @return 0=成功、負数=エラー
 */
int map_page(unsigned long vaddr, unsigned long paddr, unsigned long flags)
{
	pte_t *pte;

	/* アライメントチェック */
	if ((vaddr & ~PAGE_MASK) || (paddr & ~PAGE_MASK))
	{
		return -1;
	}

	pte = get_pte(vaddr);
	if (!pte)
	{
		printk("Cannot get PTE for vaddr 0x%08lx\n", vaddr);
		return -1;
	}

	/* ページをマップ */
	set_pte(pte, paddr, flags | _PAGE_PRESENT);

	// PTEを書き換えた後にCPUのTLBを無効化しないと，
	// 新しいマッピングがCPUに反映されず予期せぬアクセスが起こる可能性があるため
	__flush_tlb();

	return 0;
}

/** 仮想アドレスと物理アドレスを動的にマップ（vmalloc用）
 * @param vaddr 仮想アドレス（4KBアライメント）
 * @param paddr 物理アドレス（4KBアライメント）
 * @param flags ページフラグ
 * @return 0=成功、負数=エラー
 */
int map_page_vmalloc(unsigned long vaddr, unsigned long paddr, unsigned long flags)
{
	pte_t *pte_table;
	int pte_idx;

	/** ページ境界（4KB）でアラインされているかを調べる
	 * @details
	 * vaddrとpaddrの両方がページサイズの倍数であり，ページ境界に揃っていることを確認する．
	 * ページテーブルへのエントリ作成や物理フレームへのマッピングはページ単位で行う必要があるため，
	 * 仮想アドレス・物理アドレスともにページ境界で揃っていることが前提となる．
	 * @note
	 * - ~PAGE_MASK: ページ内のオフセットを取り出すマスク
	 * - vaddr & ~PAGE_MASK: vaddr のページ内のオフセット
	 *                       これが0のとき，vaddrはページサイズの倍数であり，ページ境界に揃っている
	 * - paddr & ~PAGE_MASK: paddr のページ内のオフセット
	 *                       これが0のとき，paddrはページサイズの倍数であり，ページ境界に揃っている
	 */
	if ((vaddr & ~PAGE_MASK) || (paddr & ~PAGE_MASK))
	{
		return -1;
	}

	/* vaddrから対応するページテーブルを取得または作成する */
	pte_table = get_or_create_page_table(vaddr);
	if (pte_table == NULL)
	{
		return -1;
	}

	/* PTEインデックスを計算 */
	pte_idx = pte_index(vaddr);

	/** ページをマップ
	 * @details 仮想アドレスvaddrから取得したページテーブルpte_tableのエントリpte_table[pte_idx]と
	 *          物理アドレスpaddrをマッピングする
	 */
	set_pte(&pte_table[pte_idx], paddr, flags | _PAGE_PRESENT);

	// PTEを書き換えた後にCPUのTLBを無効化しないと，
	// 新しいマッピングがCPUに反映されず予期せぬアクセスが起こる可能性があるため
	__flush_tlb();

	return 0;
}
