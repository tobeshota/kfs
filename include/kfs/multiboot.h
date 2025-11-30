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

#endif /* _KFS_MULTIBOOT_H */
