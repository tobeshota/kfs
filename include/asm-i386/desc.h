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
#define GDT_ENTRY_TSS 7

#define GDT_ENTRIES 8

/* Selectors (RPL bits appended where needed) */
#define __KERNEL_CS ((GDT_ENTRY_KERNEL_CS) << 3)
#define __KERNEL_DS ((GDT_ENTRY_KERNEL_DS) << 3)
#define __KERNEL_SS ((GDT_ENTRY_KERNEL_SS) << 3)
#define __USER_CS (((GDT_ENTRY_USER_CS) << 3) | 0x3)
#define __USER_DS (((GDT_ENTRY_USER_DS) << 3) | 0x3)
#define __USER_SS (((GDT_ENTRY_USER_SS) << 3) | 0x3)
#define __KERNEL_TSS ((GDT_ENTRY_TSS) << 3)

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
 * @note selector = __KERNEL_CS (CPL=0) を固定でセットするため，
 *       割り込み発生時に CPU が CS に __KERNEL_CS をロードし，
 *       ハンドラは常に ring-0 (特権モード) として実行される．
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

/** i386 TSS[Task State Segment]
 * @brief
 * ハードウェアコンテキストを格納するもの．
 * プロセス切り替えと特権レベル遷移のためにCPUが参照する．
 *
 * @details なぜ TSS が必要か
 * 各プロセスは「ユーザースタック」と「カーネルスタック」の2つのスタックを持つ：
 *   プロセスA のメモリ
 *   ├── ユーザースタック（ESP が指す）
 *   │   └── ユーザーモードで動いているときに使う
 *   │       pushl, popl, 関数呼び出しなど...
 *   │
 *   └── カーネルスタック（task_struct->stack が指す）
 *       └── カーネルモードで動いているときに使う
 *           割り込みハンドラ、システムコール処理など...
 *
 * ユーザースタックはユーザーが自由に書き換えられる．
 * もし割り込み時にユーザースタックをそのまま使うと，
 * 悪意あるユーザーが ESP を不正アドレスに変更したとき，割り込み発生後，
 * CPUがその不正アドレスにカーネルの情報を書き込むことができてしまう．
 * そのため，プロセスは2つのスタックを持つ必要があり，
 * 割り込み時はカーネルが管理するカーネルスタックに
 * 切り替える必要がある．その切り替えに必要なのが TSS である．
 *
 * @details CPU が TSS を参照するタイミング
 * ユーザーモード実行中に割り込み発生
 *          ↓
 * CPU「カーネルスタックはどこ？」
 *          ↓
 * CPU が TSS を参照
 *   tss.ss0（カーネルデータセグメントが格納） を SS レジスタに設定
 *   tss.esp0（カーネルスタックの末尾アドレスが格納） を ESP レジスタに設定
 *          ↓
 * カーネルスタック上で割り込みハンドラが実行される
 *
 * @details __switch_to() との連携
 * プロセス切り替え時、次のプロセスに割り込みが来た際に正しいカーネルスタックを
 * 使わせるため、__switch_to() が tss.esp0 を動的に更新する：
 *   init_tss.esp0 = next->stack + THREAD_SIZE（カーネルスタック末尾）
 *
 * @note esp0 と ss0 のみ使用。他のフィールドは Intel 仕様で必須の構造体レイアウト
 * @see Linux 2.6.11: include/asm-i386/processor.h
 */
struct tss_struct
{
	uint16_t back_link, __blh; /* 前のTSSへのリンク（未使用） */
	uint32_t esp0; /* 特権レベル0のスタックポインタ（割り込み時にCPUが ESP へロード） */
	uint16_t ss0, __ss0h; /* 特権レベル0のスタックセグメント（割り込み時にCPUが SS へロード） */
	uint32_t esp1;		  /* 特権レベル1のスタックポインタ（未使用） */
	uint16_t ss1, __ss1h;
	uint32_t esp2; /* 特権レベル2のスタックポインタ（未使用） */
	uint16_t ss2, __ss2h;
	uint32_t cr3;	 /* ページディレクトリベースレジスタ（未使用） */
	uint32_t eip;	 /* 命令ポインタ（未使用） */
	uint32_t eflags; /* フラグレジスタ（未使用） */
	uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi; /* 汎用レジスタ（未使用） */
	uint16_t es, __esh;
	uint16_t cs, __csh;
	uint16_t ss, __ssh;
	uint16_t ds, __dsh;
	uint16_t fs, __fsh;
	uint16_t gs, __gsh;
	uint16_t ldt, __ldth;	   /* LDTセグメント（未使用） */
	uint16_t trace, io_bitmap; /* デバッグとI/Oビットマップ（未使用） */
} __attribute__((packed));

/* グローバルTSS（kernel/sched/core.c または arch/i386/kernel/gdt.c で定義） */
extern struct tss_struct init_tss;

/* 外部IDTテーブル（traps.cで定義） */
extern struct idt_entry idt[];

void gdt_init(void);
void idt_init(void);
void trap_init(void);

#endif /* _ASM_I386_DESC_H */
