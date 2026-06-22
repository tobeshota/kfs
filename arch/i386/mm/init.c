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

/** カーネルのbootページディレクトリを取得する
 * @return カーネルが使用するページディレクトリ
 */
pgd_t *kernel_pgd(void)
{
	return boot_page_directory;
}

/** ページディレクトリが現在CR3にロードされているか判定する
 * @param pgd 判定対象のページディレクトリ
 * @return 現在有効なら1，それ以外は0
 */
static int pgd_is_current(pgd_t *pgd)
{
	if (!pgd)
	{
		return 0;
	}
	return (read_cr3() & PAGE_MASK) == ((unsigned long)pgd & PAGE_MASK);
}

/** 現在のページディレクトリが更新された場合のみ現在CPUのTLBを無効化する
 * @param pgd 更新したページディレクトリ
 * @note ページディレクトリを書き換えた後にCPUのTLBを無効化しないと，
 *       新しいマッピングがCPUに反映されず予期せぬアクセスが起こる可能性がある．
 */
static void flush_tlb_for_pgd(pgd_t *pgd)
{
	if (pgd_is_current(pgd))
	{
		__flush_tlb();
	}
}

/** 仮想アドレスに対応するPTEを取得する
 * @param pgd 検索対象のページディレクトリ
 * @param vaddr 仮想アドレス
 * @return PTEへのポインタ、エラー時NULL
 */
pte_t *get_pte(pgd_t *pgd, unsigned long vaddr)
{
	int pde_idx, pte_idx;
	pde_t *pde;
	pte_t *pte_table;
	unsigned long pte_table_phys;

	if (!pgd)
	{
		return NULL;
	}

	pde_idx = pgd_index(vaddr);
	pde = &pgd[pde_idx];

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
 * @param pgd    操作対象のページディレクトリ
 * @param vaddr  仮想アドレス
 * @param flags  マッピングフラグ（_PAGE_USER を含む場合、PDE にも USER ビットを設定する）
 * @return ページテーブルへのポインタ、エラー時NULL
 *
 * @note x86 ページング仕様: PDE に _PAGE_USER がないと ring-3 はその 4MB 範囲全体に
 *       アクセスできない（PTE の _PAGE_USER に関わらず）。ユーザ空間ページを
 *       マップする場合は flags に _PAGE_USER を含めること。
 */
static pte_t *get_or_create_page_table(pgd_t *pgd, unsigned long vaddr, unsigned long flags)
{
	int pde_idx;
	pde_t *pde;
	pte_t *pte_table;
	struct page *page;
	unsigned long pte_table_phys;
	unsigned long pde_flags;

	if (!pgd)
	{
		return NULL;
	}

	pde_idx = pgd_index(vaddr);
	pde = &pgd[pde_idx];

	/* ページテーブルが既に存在する場合 */
	if (pde_present(*pde))
	{
		/* 既存 PDE に USER ビットが不足していれば補完する
		 * （同じ 4MB 範囲に先にカーネルページが作成された後でユーザページを
		 *   追加する場合を想定） */
		if ((flags & _PAGE_USER) && !pde_user(*pde))
		{
			*pde |= _PAGE_USER;
			flush_tlb_for_pgd(pgd);
		}
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

	/*
	 * alloc_pages() は物理アドレスを返す。
	 * - memset / PTE 操作には仮想アドレス(__va)を使う
	 * - PDE への登録には物理アドレスをそのまま使う
	 */
	pte_table_phys = (unsigned long)page;
	pte_table = (pte_t *)__va(pte_table_phys);

	/* ページテーブルを初期化（全エントリをクリア） */
	memset(pte_table, 0, PAGE_SIZE);

	/* PDE フラグ: Present + RW は必須。ユーザページをマップする場合は USER も追加。
	 * PDE.USER=1 にしても PTE.USER=0 のページは ring-3 から保護されたまま。 */
	pde_flags = _PAGE_KERNEL;
	if (flags & _PAGE_USER)
	{
		pde_flags |= _PAGE_USER;
	}

	/* ページディレクトリエントリを設定（物理アドレスを使用） */
	set_pde(pde, pte_table_phys, pde_flags);

	return pte_table;
}

/** 仮想アドレスを物理アドレスにマップする
 * @param pgd 操作対象のページディレクトリ
 * @param vaddr 仮想アドレス（4KBアライメント）
 * @param paddr 物理アドレス（4KBアライメント）
 * @param flags ページフラグ
 * @return 0=成功、負数=エラー
 */
int map_page(pgd_t *pgd, unsigned long vaddr, unsigned long paddr, unsigned long flags)
{
	pte_t *pte_table;
	pte_t *pte;

	/* アライメントチェック */
	if ((vaddr & ~PAGE_MASK) || (paddr & ~PAGE_MASK))
	{
		return -1;
	}

	if (!pgd)
	{
		return -1;
	}

	/* vaddrから対応するページテーブルを取得または作成する
	 * flags を渡すことで、ユーザページのマップ時に PDE にも USER ビットが設定される */
	pte_table = get_or_create_page_table(pgd, vaddr, flags);
	if (pte_table == NULL)
	{
		return -1;
	}

	pte = &pte_table[pte_index(vaddr)];
	set_pte(pte, paddr, flags | _PAGE_PRESENT);
	flush_tlb_for_pgd(pgd);

	return 0;
}

/** 仮想アドレスのマッピングを解除する
 * @param pgd 操作対象のページディレクトリ
 * @param vaddr 解除する仮想アドレス（4KBアライメント）
 * @return 0=成功，負数=エラー
 * @note 物理ページと空になったページテーブルは呼び出し側が解放する。
 */
int unmap_page(pgd_t *pgd, unsigned long vaddr)
{
	pte_t *pte;

	/* ページ境界でアラインされているかを調べる */
	if (!pgd || (vaddr & ~PAGE_MASK))
	{
		return -1;
	}

	pte = get_pte(pgd, vaddr);
	if (!pte || !pte_present(*pte))
	{
		return -1;
	}

	pte_clear(pte);
	flush_tlb_for_pgd(pgd);
	return 0;
}

/** ページテーブルをコピー（fork用）
 * @param dst_pgd コピー先のページディレクトリ
 * @param src_pgd コピー元のページディレクトリ
 * @return 0=成功、負数=エラー
 * @note プロセスごとにページディレクトリを分離する。
 * @note COW（Copy On Write）は未実装
 */
int copy_page_tables(pgd_t *dst_pgd, pgd_t *src_pgd)
{
	int pde_idx;
	pde_t src_pde, *dst_pde;
	pte_t *src_pt, *dst_pt;
	struct page *new_pt_page;

	if (!dst_pgd || !src_pgd)
	{
		return -ENOMEM;
	}

	/* 全ページディレクトリエントリを走査 */
	for (pde_idx = 0; pde_idx < PTRS_PER_PGD; pde_idx++)
	{
		src_pde = src_pgd[pde_idx];

		/* ソースのPDEが存在しない場合はスキップ */
		if (!pde_present(src_pde))
		{
			continue;
		}

		/* 新しいページテーブルを割り当て */
		new_pt_page = alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
		if (!new_pt_page)
		{
			/* TODO: 既に割り当てたページテーブルをクリーンアップ */
			return -ENOMEM;
		}

		dst_pt = (pte_t *)new_pt_page;
		src_pt = (pte_t *)pde_page(src_pde);

		/* ページテーブル全体をコピーする。COW 最適化は未実装 */
		memcpy(dst_pt, src_pt, PAGE_SIZE);

		/* 新しいページテーブルをページディレクトリに設定 */
		dst_pde = &dst_pgd[pde_idx];
		set_pde(dst_pde, (unsigned long)dst_pt, pde_val(src_pde) & ~PAGE_MASK);
	}

	return 0;
}

/** ページテーブルを解放（プロセス終了時）
 * @param pgd 解放するページディレクトリ
 * @note プロセスメモリ分離のためにページディレクトリを切り替える
 */
void free_page_tables(pgd_t *pgd)
{
	int pde_idx;
	pde_t pde;
	pte_t *pt;

	if (!pgd)
	{
		return;
	}

	/* 全ページディレクトリエントリを走査 */
	for (pde_idx = 0; pde_idx < PTRS_PER_PGD; pde_idx++)
	{
		pde = pgd[pde_idx];

		/* PDEが存在しない場合はスキップ */
		if (!pde_present(pde))
		{
			continue;
		}

		/* ページテーブルを解放 */
		pt = (pte_t *)pde_page(pde);
		free_pages((struct page *)pt, 0);
	}

	/* ページディレクトリ自体を解放 */
	free_pages((struct page *)pgd, 0);
}
