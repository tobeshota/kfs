/*
 * グローバルディスクリプタテーブル（GDT）の構築とロード
 */
#include <asm-i386/desc.h>
#include <kfs/printk.h>
#include <kfs/stdint.h>

/* GDTを配置する物理アドレス */
#define KFS_GDT_PHYS 0x00000800U

/* 64ビットのセグメントディスクリプタ値を生成する．CPUがメモリ領域の属性を認識するために必要 */
static uint64_t make_seg_desc(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
	/* limit: 20ビット（粒度4Kの場合、実効値は(limit<<12)+0xFFF） */
	uint64_t desc = 0;
	desc = (limit & 0xFFFFULL);						 /* リミット下位16ビット */
	desc |= ((uint64_t)(base & 0xFFFFFF)) << 16;	 /* ベースアドレス 23:0 */
	desc |= ((uint64_t)access) << 40;				 /* アクセスバイト（権限・タイプ） */
	desc |= ((uint64_t)((limit >> 16) & 0xF)) << 48; /* リミット上位4ビット */
	desc |= ((uint64_t)(flags & 0xF)) << 52;		 /* フラグ4ビット（G D L AVL） */
	desc |= ((uint64_t)((base >> 24) & 0xFF)) << 56; /* ベースアドレス 31:24 */
	return desc;
}

/* アクセスバイトの構成要素（Present + DPL + S + type） */
#define ACC_CODE_EXRD 0x0A /* 実行/読み取り可能なコードセグメント */
#define ACC_DATA_RDWR 0x02 /* 読み取り/書き込み可能なデータセグメント */
#define ACC_S 0x10		   /* ディスクリプタタイプ（コード/データ） */
#define ACC_P 0x80		   /* Present（セグメントが有効） */

/* フラグニブル: |G|D/B|L|AVL| => G=1(4K粒度), D=1(32ビット), L=0, AVL=0 -> 1100b (0xC) */
#define SEG_FLAG_GRAN_4K_32 0xC

/* コードセグメント用のアクセスバイトを生成．実行権限を付与するために必要 */
static inline uint8_t access_code(uint8_t dpl)
{
	return ACC_P | (dpl << 5) | ACC_S | ACC_CODE_EXRD;
}
/* データセグメント用のアクセスバイトを生成．読み書き権限を付与するために必要 */
static inline uint8_t access_data(uint8_t dpl)
{
	return ACC_P | (dpl << 5) | ACC_S | ACC_DATA_RDWR;
}

/* GDTを初期化してCPUに宣言する．メモリセグメント管理を有効化するために必要 */
void gdt_init(void)
{
	/* フラットメモリモデル: base=0, limit=0xFFFFF（4K粒度で実質4GiB全域をカバー） */
	const uint32_t base = 0x00000000;
	const uint32_t limit = 0x000FFFFF; /* 20ビット最大値 */

	/* 一時バッファでGDTエントリを構築（7エントリ: NULL + カーネル3 + ユーザ3） */
	uint64_t gdt_build[GDT_ENTRIES];
	gdt_build[GDT_ENTRY_NULL] = 0x0000000000000000ULL; /* NULLディスクリプタ（x86仕様で必須） */
	gdt_build[GDT_ENTRY_KERNEL_CS] =
		make_seg_desc(base, limit, access_code(0), SEG_FLAG_GRAN_4K_32); /* カーネルコード（DPL=0） */
	gdt_build[GDT_ENTRY_KERNEL_DS] =
		make_seg_desc(base, limit, access_data(0), SEG_FLAG_GRAN_4K_32); /* カーネルデータ（DPL=0） */
	gdt_build[GDT_ENTRY_KERNEL_SS] =
		make_seg_desc(base, limit, access_data(0), SEG_FLAG_GRAN_4K_32); /* カーネルスタック専用セグメント（DPL=0） */
	gdt_build[GDT_ENTRY_USER_CS] =
		make_seg_desc(base, limit, access_code(3), SEG_FLAG_GRAN_4K_32); /* ユーザコード（DPL=3） */
	gdt_build[GDT_ENTRY_USER_DS] =
		make_seg_desc(base, limit, access_data(3), SEG_FLAG_GRAN_4K_32); /* ユーザデータ（DPL=3） */
	gdt_build[GDT_ENTRY_USER_SS] =
		make_seg_desc(base, limit, access_data(3), SEG_FLAG_GRAN_4K_32); /* ユーザスタック専用セグメント（DPL=3） */

	/* 仕様で要求された物理アドレス0x800にGDTをコピー */
	volatile uint64_t *gdt_phys = (volatile uint64_t *)KFS_GDT_PHYS;
	for (int i = 0; i < GDT_ENTRIES; ++i)
		gdt_phys[i] = gdt_build[i];

	/* GDTR（GDTレジスタ）用の構造体を準備 */
	struct desc_ptr gdtp;
	gdtp.size = (uint16_t)(GDT_ENTRIES * 8 - 1); /* GDTサイズ-1（CPUの仕様） */
	gdtp.address = (uint32_t)KFS_GDT_PHYS;		 /* GDTの物理アドレス */

	/* GDTRをロードし、セグメントレジスタを新しいセレクタで再ロード．far jumpでCSを更新 */
	asm volatile("lgdt %[gdtp]\n\t" /* GDTRにGDTのアドレスとサイズを登録 */
				 "mov %[ds_sel], %%ax\n\t"
				 "mov %%ax, %%ds\n\t" /* データセグメントレジスタを更新 */
				 "mov %%ax, %%es\n\t" /* 拡張セグメントレジスタを更新 */
				 "mov %%ax, %%fs\n\t" /* 追加セグメントレジスタFSを更新 */
				 "mov %%ax, %%gs\n\t" /* 追加セグメントレジスタGSを更新 */
				 "mov %[ss_sel], %%ax\n\t"
				 "mov %%ax, %%ss\n\t" /* スタックセグメントレジスタを更新 */
				 /* far jumpでCSを強制リロード（CPUの命令キャッシュをフラッシュ） */
				 "ljmp %[cs_sel], $1f\n\t"
				 "1:\n\t"
				 :
				 : [gdtp] "m"(gdtp), [ds_sel] "r"((uint16_t)__KERNEL_DS), [ss_sel] "r"((uint16_t)__KERNEL_SS),
				   [cs_sel] "i"(__KERNEL_CS)
				 : "ax");
}
