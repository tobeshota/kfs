#ifndef _KFS_MULTIBOOT_H
#define _KFS_MULTIBOOT_H

/**
 * ブートローダー（GRUB）から渡されるメモリ情報を取得する
 */

#include <kfs/stdint.h>

/* Multiboot情報構造体のフラグ */
#define MULTIBOOT_INFO_MEMORY 0x00000001  /* mem_lower, mem_upperが有効 */
#define MULTIBOOT_INFO_MEM_MAP 0x00000040 /* mmap_*が有効 */

/* メモリマップエントリの型 */
#define MULTIBOOT_MEMORY_AVAILABLE 1 /* 使用可能なRAM */
#define MULTIBOOT_MEMORY_RESERVED 2	 /* 予約済み（使用不可） */

/** Multiboot情報構造体（必要な部分のみ）
 * @see Section 3.3 "Boot information format" of
 *      https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */
struct multiboot_info
{
	uint32_t flags;		  /* 有効なフィールドを示すフラグ */
	uint32_t mem_lower;	  /* 下位メモリのKB数（0-640KB） */
	uint32_t mem_upper;	  /* 上位メモリのKB数（1MB以降） */
	uint32_t boot_device; /* ブートデバイス */
	uint32_t cmdline;	  /* カーネルコマンドライン */
	uint32_t mods_count;  /* モジュール数 */
	uint32_t mods_addr;	  /* モジュールリストアドレス */
	uint32_t syms[4];	  /* シンボル情報 */
	uint32_t mmap_length; /* メモリマップの長さ */
	uint32_t mmap_addr;	  /* メモリマップのアドレス */
} __attribute__((packed));

/* メモリマップエントリ */
struct multiboot_mmap_entry
{
	uint32_t size; /* エントリのサイズ（このフィールドを除く） */
	uint64_t addr; /* ベースアドレス */
	uint64_t len;  /* 領域の長さ */
	uint32_t type; /* 領域のタイプ */
} __attribute__((packed));

/** Multiboot2 (UEFI対応) 構造体定義
 * @see Section 3.6 "Boot information" of
 *      https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
 */
/* GRUBがMultiboot2で起動したときにEAXに設定するマジックナンバー */
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

/* Multiboot2タグタイプ */
#define MULTIBOOT2_TAG_TYPE_END 0  /* タグリスト終端 */
#define MULTIBOOT2_TAG_TYPE_MMAP 6 /* メモリマップ */

/* Multiboot2メモリマップエントリタイプ（Multiboot1と共通の値） */
#define MULTIBOOT2_MEMORY_AVAILABLE 1 /* 使用可能なRAM */
#define MULTIBOOT2_MEMORY_RESERVED 2  /* 予約済み（使用不可） */

/** Multiboot2情報ヘッダ
 * タグリストの先頭 8 バイト。その直後からタグが続く。
 */
struct multiboot2_info
{
	uint32_t total_size; /* ヘッダ自身を含む情報全体のサイズ（バイト） */
	uint32_t reserved;	 /* 予約（0） */
} __attribute__((packed));

/** Multiboot2汎用タグヘッダ
 * すべてのタグはこのヘッダで始まり、次のタグは 8 バイト境界に整列する。
 */
struct multiboot2_tag
{
	uint32_t type; /* タグタイプ（MULTIBOOT2_TAG_TYPE_*） */
	uint32_t size; /* タグ全体のサイズ（このヘッダを含む） */
} __attribute__((packed));

/** Multiboot2メモリマップエントリ */
struct multiboot2_mmap_entry
{
	uint64_t addr; /* ベースアドレス */
	uint64_t len;  /* 領域の長さ（バイト） */
	uint32_t type; /* 領域タイプ（MULTIBOOT2_MEMORY_*） */
	uint32_t zero; /* 予約（0） */
} __attribute__((packed));

/** Multiboot2メモリマップタグ（type=6） */
struct multiboot2_tag_mmap
{
	uint32_t type;			/* MULTIBOOT2_TAG_TYPE_MMAP (6) */
	uint32_t size;			/* タグ全体のサイズ */
	uint32_t entry_size;	/* 各エントリのサイズ（通常24バイト） */
	uint32_t entry_version; /* エントリフォーマットバージョン（0） */
							/* 以降 entry_size バイトごとに multiboot2_mmap_entry が続く */
} __attribute__((packed));

#endif /* _KFS_MULTIBOOT_H */
