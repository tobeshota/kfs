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
 * @param vaddr  仮想アドレス
 * @param flags  マッピングフラグ（_PAGE_USER を含む場合、PDE にも USER ビットを設定する）
 * @return ページテーブルへのポインタ、エラー時NULL
 *
 * @note x86 ページング仕様: PDE に _PAGE_USER がないと ring-3 はその 4MB 範囲全体に
 *       アクセスできない（PTE の _PAGE_USER に関わらず）。ユーザ空間ページを
 *       マップする場合は flags に _PAGE_USER を含めること。
 */
static pte_t *get_or_create_page_table(unsigned long vaddr, unsigned long flags)
{
	int pde_idx;
	pde_t *pde;
	pte_t *pte_table;
	struct page *page;
	unsigned long pte_table_phys;
	unsigned long pde_flags;

	pde_idx = pgd_index(vaddr);
	pde = &boot_page_directory[pde_idx];

	/* ページテーブルが既に存在する場合 */
	if (pde_present(*pde))
	{
		/* 既存 PDE に USER ビットが不足していれば補完する
		 * （同じ 4MB 範囲に先にカーネルページが作成された後でユーザページを
		 *   追加する場合を想定） */
		if ((flags & _PAGE_USER) && !pde_user(*pde))
		{
			*pde |= _PAGE_USER;
			__flush_tlb();
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

	/* vaddrから対応するページテーブルを取得または作成する
	 * flags を渡すことで、ユーザページのマップ時に PDE にも USER ビットが設定される */
	pte_table = get_or_create_page_table(vaddr, flags);
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

/** ページテーブルをコピー（fork用）
 * @param dst_pgd コピー先のページディレクトリ
 * @param src_pgd コピー元のページディレクトリ
 * @return 0=成功、負数=エラー
 * @note Phase 4: プロセスメモリ分離
 * @note Phase 6でCOW（Copy On Write）を実装予定
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

		/* ページテーブル全体をコピー（Phase 6でCOW最適化予定） */
		memcpy(dst_pt, src_pt, PAGE_SIZE);

		/* 新しいページテーブルをページディレクトリに設定 */
		dst_pde = &dst_pgd[pde_idx];
		set_pde(dst_pde, (unsigned long)dst_pt, pde_val(src_pde) & ~PAGE_MASK);
	}

	return 0;
}

/** ページテーブルを解放（プロセス終了時）
 * @param pgd 解放するページディレクトリ
 * @note Phase 4: プロセスメモリ分離
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
