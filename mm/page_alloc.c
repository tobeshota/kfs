/**
 * Physical Page Allocator
 *
 * ビットマップベースの物理ページアロケータ
 * - Multiboot情報からメモリマップを解析
 * - カーネル領域を予約
 * - ページ単位での割り当て・解放
 */

#include <asm-i386/page.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/string.h>

/* ページビットマップ（最大128MB = 32768ページ = 4096バイト） */
#define MAX_PAGES 32768
static uint8_t page_bitmap[MAX_PAGES / 8];

/* メモリ統計情報 */
unsigned long total_pages = 0;
unsigned long nr_free_pages = 0; /* 空きページ数（Linux 2.6.11に倣い nr_ プレフィックス） */
unsigned long kernel_end_pfn = 0;
static int page_alloc_initialized = 0; /* 初期化済みフラグ */

/* 外部シンボル：カーネルの終端アドレス（linker.ldで定義） */
extern char _kernel_end[];

/**
 * ページフレーム番号（PFN）に対応するビットを設定
 */
static inline void set_page_bit(unsigned long pfn)
{
	if (pfn >= MAX_PAGES)
	{
		return;
	}
	page_bitmap[pfn / 8] |= (1 << (pfn % 8));
}

/**
 * ページフレーム番号（PFN）に対応するビットをクリア
 */
static inline void clear_page_bit(unsigned long pfn)
{
	if (pfn >= MAX_PAGES)
	{
		return;
	}
	page_bitmap[pfn / 8] &= ~(1 << (pfn % 8)); // pfn番目のビットに0を書き込む
}

/**
 * ページフレーム番号（PFN）に対応するビットをテスト
 * @return 1: 使用中, 0: 未使用
 */
static inline int test_page_bit(unsigned long pfn)
{
	if (pfn >= MAX_PAGES)
	{
		return 1; /* 範囲外は使用中とみなす */
	}
	return (page_bitmap[pfn / 8] & (1 << (pfn % 8))) != 0;
}

/**
 * Multiboot情報からメモリマップを解析し、使用可能なページを初期化
 */
static void parse_memory_map(struct multiboot_info *mbi)
{
	struct multiboot_mmap_entry *mmap;
	unsigned long mmap_end;

	/* メモリマップが利用可能かチェック */
	if (!(mbi->flags & MULTIBOOT_INFO_MEM_MAP))
	{
		panic("Multiboot memory map not available");
	}

	printk("Memory map:\n");

	mmap = (struct multiboot_mmap_entry *)mbi->mmap_addr;
	mmap_end = mbi->mmap_addr + mbi->mmap_length;

	/* 全ページを使用中としてマーク（デフォルト） */
	memset(page_bitmap, 0xFF, sizeof(page_bitmap));

	/* メモリマップを走査 */
	while ((unsigned long)mmap < mmap_end)
	{
		uint64_t addr = mmap->addr;
		uint64_t len = mmap->len;
		uint32_t type = mmap->type;

		printk("  [0x");
		printk((type == MULTIBOOT_MEMORY_AVAILABLE) ? " available]\n" : " reserved]\n");

		/* 使用可能なメモリ領域のみをマーク */
		if (type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			unsigned long start_pfn = addr / PAGE_SIZE;
			unsigned long end_pfn = (addr + len) / PAGE_SIZE;

			/** カーネルが使用するページ以降のページを使用可能とする
			 * @note カーネル自身が使用しているページが上書きされないようにするため
			 */
			if (start_pfn < kernel_end_pfn)
			{
				start_pfn = kernel_end_pfn;
			}

			for (unsigned long pfn = start_pfn; pfn < end_pfn && pfn < MAX_PAGES; pfn++)
			{
				clear_page_bit(pfn); /* 未使用としてマーク */
				nr_free_pages++;
			}

			if (end_pfn > total_pages && end_pfn <= MAX_PAGES)
			{
				total_pages = end_pfn;
			}
		}

		/* 次のエントリへ */
		mmap = (struct multiboot_mmap_entry *)((unsigned long)mmap + mmap->size + sizeof(mmap->size));
	}

	printk("Memory init complete\n");
}

/**
 * 物理ページアロケータの初期化
 * @param mbi Multiboot情報構造体へのポインタ
 */
void page_alloc_init(struct multiboot_info *mbi)
{
	/* 既に初期化済みなら何もしない */
	if (page_alloc_initialized)
	{
		return;
	}

	printk("page_alloc_init() called\n");

	/* カーネルの終端アドレスを取得（ページ境界に切り上げ） */
	unsigned long kernel_end = (unsigned long)_kernel_end;
	kernel_end_pfn = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;

	printk("Initializing page allocator...\n");
	printk("Kernel end address calculated\n");

	/* メモリマップを解析 */
	printk("About to parse memory map\n");
	parse_memory_map(mbi);
	printk("Memory map parsed\n");

	page_alloc_initialized = 1;
}

/**
 * 物理ページを1ページ割り当てる
 * @param gfp_mask GFPフラグ
 * @return 割り当てられたページの物理アドレス（失敗時は0）
 */
unsigned long __alloc_pages(unsigned int gfp_mask)
{
	unsigned long pfn;

	/** 空きページを検索
	 * @note カーネルが使用するページを割当対象から除外する
	 */
	for (pfn = kernel_end_pfn; pfn < total_pages && pfn < MAX_PAGES; pfn++)
	{
		if (!test_page_bit(pfn))
		{
			/* ページを使用中としてマーク */
			set_page_bit(pfn);
			nr_free_pages--;

			/* 物理アドレスを計算 */
			unsigned long phys_addr = pfn * PAGE_SIZE;

			/* GFP_ZEROフラグが設定されている場合はゼロクリア */
			if (gfp_mask & GFP_ZERO)
			{
				memset((void *)phys_addr, 0, PAGE_SIZE);
			}

			return phys_addr;
		}
	}

	/* 割り当て失敗 */
	return 0;
}

/**
 * 物理ページを解放する
 * @param addr ページの物理アドレス
 */
void __free_pages(unsigned long addr)
{
	unsigned long pfn = addr / PAGE_SIZE;

	/** 範囲チェック
	 * @note カーネルが使用するページを解放対象から除外する
	 */
	if (pfn < kernel_end_pfn || pfn >= total_pages || pfn >= MAX_PAGES)
	{
		printk(KERN_WARNING "Attempt to free invalid page: 0x%08lx (PFN: %lu)\n", addr, pfn);
		return;
	}

	/* 既に解放済みかチェック */
	if (!test_page_bit(pfn))
	{
		printk(KERN_WARNING "Double free detected: 0x%08lx (PFN: %lu)\n", addr, pfn);
		return;
	}

	/* ページを未使用としてマーク */
	clear_page_bit(pfn);
	nr_free_pages++;
}

/**
 * alloc_pages - 物理ページを割り当てる（Linux 2.6.11互換ラッパー）
 * @gfp_mask: GFPフラグ
 * @order: ページオーダー（0 = 1ページ、1 = 2ページ、...）
 * @return: ページ構造体へのポインタ（物理アドレスとして使用可能）
 *
 * Linux 2.6.11では struct page* を返すが、kfs-3では簡略化のため
 * 物理アドレスをそのままポインタとして返す
 */
struct page *alloc_pages(unsigned int gfp_mask, unsigned int order)
{
	unsigned long addr;

	/* order > 0はkfs-3では未対応 */
	if (order != 0)
	{
		printk(KERN_WARNING "alloc_pages: order %u not supported\n", order);
		return NULL;
	}

	/* 1ページ割り当て */
	addr = __alloc_pages(gfp_mask);
	if (addr == 0)
	{
		return NULL;
	}

	/* 物理アドレスをポインタとして返す */
	return (struct page *)addr;
}

/**
 * free_pages - 物理ページを解放する（Linux 2.6.11互換ラッパー）
 * @page: ページ構造体へのポインタ（物理アドレス）
 * @order: ページオーダー（0 = 1ページ、1 = 2ページ、...）
 */
void free_pages(struct page *page, unsigned int order)
{
	/* order > 0はkfs-3では未対応 */
	if (order != 0)
	{
		printk(KERN_WARNING "free_pages: order %u not supported\n", order);
		return;
	}

	/* 物理アドレスとして解放 */
	__free_pages((unsigned long)page);
}

/**
 * メモリ統計情報を表示
 */
void show_mem_info(void)
{
	printk("Memory statistics:\n");
	printk("  Total pages (approx)\n");
	printk("  Free pages (approx)\n");
}

/**
 * メモリ管理システムの初期化
 * mm.hで宣言されている関数の実装
 */
void mem_init(void)
{
	printk("Memory initialization complete\n");
	show_mem_info();
}

/**
 * テスト用: ページアロケータを初期状態にリセット
 * @details
 * ページビットマップを初期化直後の状態に完全リセットする。
 * テストの独立性を保証するために、各テスト前に呼び出す。
 *
 * リセット内容:
 * - ページビットマップを初期状態（カーネル領域以外は空き）に戻す
 * - 空きページ数カウンタをリセット
 */
void page_allocator_reset_for_test(void)
{
	unsigned long pfn;

	/* 初期化されていなければ何もしない */
	if (!page_alloc_initialized)
	{
		return;
	}

	/* 全ページを空きとしてマーク */
	memset(page_bitmap, 0x00, sizeof(page_bitmap));

	/* カーネル使用領域を使用中としてマーク */
	for (pfn = 0; pfn < kernel_end_pfn && pfn < MAX_PAGES; pfn++)
	{
		set_page_bit(pfn);
	}

	/* 空きページ数をリセット */
	nr_free_pages = total_pages - kernel_end_pfn;
}
