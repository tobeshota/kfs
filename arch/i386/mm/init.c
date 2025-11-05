#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/string.h>

/* ページディレクトリ（4KB、1024エントリ） */
static pde_t boot_page_directory[1024] __attribute__((aligned(4096)));

/** 最初のページテーブル（0-4MBをマップ）
 * @note "_0"は最初の（0番目の）ページテーブルであることを示す
 */
static pte_t boot_page_table_0[1024] __attribute__((aligned(4096)));

/** 仮想アドレスと物理アドレスが1対1で対応する（同じ値になる）ようにページテーブルを設定する
 * 恒等写像の範囲: 0-8MB（カーネルイメージ + 初期カーネルヒープ領域）
 */
static void setup_identity_mapping(void)
{
	unsigned long addr;
	int pde_idx, pte_idx;

	printk("Setting up identity mapping (0-8MB)...\n");

	/* ページディレクトリを初期化 */
	memset(boot_page_directory, 0, sizeof(boot_page_directory));

	/* 最初のページテーブル（0-4MB）を初期化 */
	memset(boot_page_table_0, 0, sizeof(boot_page_table_0));

	/* 0-4MBの恒等マッピングを設定 */
	for (addr = 0; addr < 0x400000; addr += PAGE_SIZE)
	{
		pte_idx = pte_index(addr);
		set_pte(&boot_page_table_0[pte_idx], addr, _PAGE_PRESENT | _PAGE_RW);
	}

	/* ページディレクトリの最初のエントリに上記テーブルを設定 */
	pde_idx = pgd_index(0);
	set_pde(&boot_page_directory[pde_idx], (unsigned long)boot_page_table_0, _PAGE_PRESENT | _PAGE_RW);

	printk("Identity mapping established\n");
}

/**
 * ページディレクトリをCR3にロード
 */
static void load_page_directory(void)
{
	unsigned long pgdir_phys;

	pgdir_phys = (unsigned long)boot_page_directory;

	printk("Loading page directory at 0x%08lx\n", pgdir_phys);
	load_cr3(pgdir_phys);

	printk("Page directory loaded\n");
}

/** ページングサブシステムの初期化
 * ページディレクトリ/テーブルの作成
 * 恒等マッピングの設定
 * CR3レジスタへのページディレクトリ設定
 * CR0でページングを有効化
 */
void paging_init(void)
{
	printk("Initializing paging subsystem...\n");

	/* 恒等マッピングを設定 */
	setup_identity_mapping();

	/* CR3にページディレクトリをロード */
	load_page_directory();

	/* ページングを有効化 */
	printk("Enabling paging...\n");
	enable_paging();

	printk("Paging enabled successfully\n");
	printk("Virtual address space initialized\n");
}

/** 仮想アドレスに対応するPTEを取得する
 * @param vaddr 仮想アドレス
 * @return PTEへのポインタ、エラー時NULL
 */
pte_t *get_pte(unsigned long vaddr)
{
	int pde_idx, pte_idx;
	pde_t *pde;
	pte_t *pte_table;

	pde_idx = pgd_index(vaddr);
	pde = &boot_page_directory[pde_idx];

	/* ページディレクトリエントリが存在するかチェック */
	if (!pde_present(*pde))
	{
		return NULL;
	}

	/* ページテーブルを取得 */
	pte_table = (pte_t *)pde_page(*pde);
	pte_idx = pte_index(vaddr);

	return &pte_table[pte_idx];
}

/** 仮想アドレスを物理アドレスにマップ
 *
 * @vaddr: 仮想アドレス（4KBアライメント）
 * @paddr: 物理アドレス（4KBアライメント）
 * @flags: ページフラグ
 * @return: 0=成功、負数=エラー
 */
int map_page(unsigned long vaddr, unsigned long paddr, unsigned long flags)
{
	pte_t *pte;

	/* アライメントチェック */
	if ((vaddr & ~PAGE_MASK) || (paddr & ~PAGE_MASK))
	{
		return -1;
	}

	/* 既存実装では恒等マッピングのため、簡単な検証のみ */
	if (vaddr != paddr)
	{
		printk("Non-identity mapping not supported yet\n");
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
