/*
 * Minimal i386 GDT setup header
 */
#ifndef _ASM_I386_DESC_H
#define _ASM_I386_DESC_H

#include <kfs/stdint.h>

/* GDT entry indices (like Linux GDT_ENTRY_*) */
#define GDT_ENTRY_NULL 0
#define GDT_ENTRY_KERNEL_CS 1
#define GDT_ENTRY_KERNEL_DS 2
#define GDT_ENTRY_KERNEL_SS 3
#define GDT_ENTRY_USER_CS 4
#define GDT_ENTRY_USER_DS 5
#define GDT_ENTRY_USER_SS 6

#define GDT_ENTRIES 7

/* Selectors (RPL bits appended where needed) */
#define __KERNEL_CS ((GDT_ENTRY_KERNEL_CS) << 3)
#define __KERNEL_DS ((GDT_ENTRY_KERNEL_DS) << 3)
#define __KERNEL_SS ((GDT_ENTRY_KERNEL_SS) << 3)
#define __USER_CS (((GDT_ENTRY_USER_CS) << 3) | 0x3)
#define __USER_DS (((GDT_ENTRY_USER_DS) << 3) | 0x3)
#define __USER_SS (((GDT_ENTRY_USER_SS) << 3) | 0x3)

/* Descriptor pointer structure (lgdt/lidt operand) */
struct desc_ptr
{
	uint16_t size;	  /* size - 1 */
	uint32_t address; /* linear address */
} __attribute__((packed));

/** IDT(Interrupt Descriptor Table)エントリ構造体(8バイト)
 * @note Intel i386 割り込みゲートディスクリプタ形式
 * @see Linux 2.6.11: include/asm-i386/desc.h
 */
struct idt_entry
{
	uint16_t base_lo;  /* ハンドラアドレスの下位16ビット */
	uint16_t selector; /* カーネルコードセグメントセレクタ */
	uint8_t zero;	   /* 常に0 */
	uint8_t flags;	   /* タイプとDPL (P=1, DPL, Type) */
	uint16_t base_hi;  /* ハンドラアドレスの上位16ビット */
} __attribute__((packed));

/** IDTゲートタイプ（flags用）
 * @details ビット構成:
 *   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+
 * | P |  DPL  | 0 |    Type       |
 * +---+---+---+---+---+---+---+---+
 * - P: ディスクリプタが物理メモリに存在する (1: 存在する, 0: 存在しない)
 * - DPL: Discriptor Privilege Level (0: kernel mode, 3: user mode)
 * - Type: Gate Type
 */
#define IDT_GATE_INTERRUPT 0x8E /* P=1, DPL=0, 32-bit Interrupt Gate */
#define IDT_GATE_TRAP 0x8F		/* P=1, DPL=0, 32-bit Trap Gate */
#define IDT_GATE_USER 0xEE		/* P=1, DPL=3, 32-bit Interrupt Gate (ユーザーから呼び出し可) */

/* IDTエントリ数（i386: 256エントリ） */
#define IDT_ENTRIES 256

/** IDTゲートを設定するマクロ
 * @param idt_table IDTテーブルへのポインタ
 * @param n         割り込み番号 (0-255)
 * @param addr      ハンドラ関数のアドレス
 * @param type_attr ゲートタイプとDPL (IDT_GATE_*)
 */
#define _set_gate(idt_table, n, addr, type_attr)                                                                       \
	do                                                                                                                 \
	{                                                                                                                  \
		uint32_t __base = (uint32_t)(addr);                                                                            \
		(idt_table)[n].base_lo = __base & 0xFFFF;                                                                      \
		(idt_table)[n].base_hi = (__base >> 16) & 0xFFFF;                                                              \
		(idt_table)[n].selector = __KERNEL_CS;                                                                         \
		(idt_table)[n].zero = 0;                                                                                       \
		(idt_table)[n].flags = (type_attr);                                                                            \
	} while (0)

/** Interrupt Vector番号とISRアドレスを対応づける
 * @param n    Interrupt Vector番号 (Interrupt Descriptor Tableのインデックス)
 * @param addr ISR (Interrupt Service Routine) のアドレス
 * @details    CPUがInterrupt Vector番号を受け取ったとき，
 *             どのISRのアドレスにジャンプするかを設定する
 * IDT (Interrupt Descriptor Table)
 * ┌─────────────────────────────┬─────────────────────────────────────┐
 * │ Interrupt Vector number (n) │ ISR address (addr)                  │
 * ├─────────────────────────────┼─────────────────────────────────────┤
 * │    0x00                     │   divide_error's address            │
 * │    0x01                     │   debug's address                   │
 * │    ...                      │   ...                               │
 * │    0x21                     │   irq1's address                    │
 * │    ...                      │   ...                               │
 * └─────────────────────────────┴─────────────────────────────────────┘
 * @note set_intr_gate() は set interrupt gate の略
 * @note init_8259A(): IRQ番号とInterrupt Vector番号を対応づける
 */
#define set_intr_gate(n, addr) _set_gate(idt, n, addr, IDT_GATE_INTERRUPT)
#define set_system_gate(n, addr) _set_gate(idt, n, addr, IDT_GATE_USER)
#define set_trap_gate(n, addr) _set_gate(idt, n, addr, IDT_GATE_TRAP)

/* 外部IDTテーブル（traps.cで定義） */
extern struct idt_entry idt[];

void gdt_init(void);
void idt_init(void);
void trap_init(void);

#endif /* _ASM_I386_DESC_H */
