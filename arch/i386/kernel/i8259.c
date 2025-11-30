/**
 * 8259A PIC (Programmable Interrupt Controller) driver
 * 8259A PICの初期化とIRQ制御を行う
 */

#include <asm-i386/desc.h>
#include <asm-i386/i8259.h>
#include <asm-i386/io.h>
#include <kfs/stdint.h>

/* IRQハンドラのエントリポイント（entry.Sで定義） */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

/* 外部IDTテーブル（traps.cで定義） */
extern struct idt_entry idt[];

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

/** 8259A PIC IRQマスクキャッシュ
 * @details ビットが1のIRQはマスク(無効)、0はアンマスク(有効)
 *          初期状態: 全IRQをマスク (0xFFFF)
 */
static uint16_t cached_irq_mask = 0xFFFF;

/* マスターPIC用マスク (下位8ビット) */
#define cached_master_mask ((uint8_t)(cached_irq_mask & 0xFF))
/* スレーブPIC用マスク (上位8ビット) */
#define cached_slave_mask ((uint8_t)((cached_irq_mask >> 8) & 0xFF))

/** 指定IRQを無効化する
 * @param irq IRQ番号 (0-15)
 * @note ハードウェアからの割り込みを一時的に止める
 */
void disable_8259A_irq(unsigned int irq)
{
	unsigned int mask = 1 << irq;

	/* キャッシュを更新してPICに書き込む */
	cached_irq_mask |= mask;
	if (irq < 8)
	{
		/* マスターPIC (IRQ0-7) */
		outb(PIC_MASTER_IMR, cached_master_mask);
	}
	else
	{
		/* スレーブPIC (IRQ8-15) */
		outb(PIC_SLAVE_IMR, cached_slave_mask);
	}
}

/** 指定IRQを有効化する
 * @param irq IRQ番号 (0-15)
 * @note ハードウェアからの割り込みを受け付け可能にする
 */
void enable_8259A_irq(unsigned int irq)
{
	unsigned int mask = ~(1 << irq);

	/* キャッシュを更新してPICに書き込む */
	cached_irq_mask &= mask;
	if (irq < 8)
	{
		/* マスターPIC (IRQ0-7) */
		outb(PIC_MASTER_IMR, cached_master_mask);
	}
	else
	{
		/* スレーブPIC (IRQ8-15) */
		outb(PIC_SLAVE_IMR, cached_slave_mask);
	}
}

/** IRQをマスクしてEOI (End Of Interrupt) を送信する
 * @param irq IRQ番号 (0-15)
 * @note 割り込みハンドラの最後に呼び出し、PICに処理完了を通知
 */
void mask_and_ack_8259A(unsigned int irq)
{
	unsigned int irqmask = 1 << irq;

	/* IRQをマスク */
	cached_irq_mask |= irqmask;

	if (irq < 8)
	{
		/* マスターPIC: マスク更新 + EOI送信 */
		outb(PIC_MASTER_IMR, cached_master_mask);
		outb(PIC_MASTER_CMD, PIC_EOI);
	}
	else
	{
		/* スレーブPIC: マスク更新 + 両PICにEOI送信 */
		outb(PIC_SLAVE_IMR, cached_slave_mask);
		outb(PIC_SLAVE_CMD, PIC_EOI);
		/* カスケード接続のため、マスターにもEOI必要 */
		outb(PIC_MASTER_CMD, PIC_EOI);
	}
}

/** 8259A PICを初期化する
 * @details マスタPIC/スレーブPICのICW (Initialization Command Word) を初期化する
 *   ICW1: 初期化開始，エッジトリガ，カスケード接続あり
 *   ICW2: IRQ番号とInterrupt Vector番号を対応づける
 *   ICW3: カスケード接続設定
 *   ICW4: 8086モード
 * @note set_intr_gate(): Interrupt Vector番号とISRアドレスを対応づける
 * @see https://stanislavs.org/helppc/8259.html
 */
void init_8259A(void)
{
	/* 全IRQをマスク (初期化中は割り込み禁止) */
	cached_irq_mask = 0xFFFF;
	outb(PIC_MASTER_IMR, cached_master_mask);
	outb(PIC_SLAVE_IMR, cached_slave_mask);

	/*
	 * マスターPIC (8259A-1) 初期化
	 */

	/** ICW1: マスターPIC (8259A-1)に初期化の開始を伝える 0x11 = 0001 0001
	 * bit7-5: 0 = must be zero for PC systems
	 * bit4:   1 = must be 1 for ICW1 (初期化コマンド)
	 * bit3:   0 = edge triggered mode (8259Aの標準モードであるため)
	 * bit2:   0 = 8 byte interrupt vectors (デフォルト設定)
	 * bit1:   0 = cascading 8259's (スレーブあり)
	 * bit0:   1 = ICW4 is needed
	 */
	outb(PIC_MASTER_CMD, 0x11);
	/** ICW2: IRQ番号とInterrupt Vector番号を対応づける 0x20 = 0010 0000
	 * bit7-3: 0x04 = A7-A3 of interrupt vector
	 * bit2-0: 0 = must be 000 on 80x86 systems
	 * 結果: (IRQ0, INT 0x20), (IRQ1, INT 0x21), ..., (IRQ7, INT 0x27)
	 */
	outb(PIC_MASTER_IMR, IRQ0_VECTOR);
	/** ICW3: スレーブPICがマスタPICの何番のIRQラインに接続するかを設定する 0x04 = 0000 0100
	 * bit2: 1 = マスターPICのIRQ2にスレーブPICが接続する
	 * 他:   0 = no slave
	 * @note マスターPICでは，スレーブPICが接続するIRQライン番号をビットマスクで指定する
	 */
	outb(PIC_MASTER_IMR, 1 << PIC_CASCADE_IR);
	/** ICW4: 動作モードを設定する 0x01 = 0000 0001
	 * bit7-5: 0  = unused (set to zero)
	 * bit4:   0  = Fully Nested Mode
	 * bit3-2: 00 = not buffered
	 * bit1:   0  = EOI (End Of Interrupt) を手動で送信
	 * bit0:   1  = 80x86 mode
	 */
	outb(PIC_MASTER_IMR, MASTER_ICW4_DEFAULT);

	/*
	 * スレーブPIC (8259A-2) 初期化
	 */

	/** ICW1: 初期化開始 0x11 = 0001 0001
	 * bit7-5: 0 = must be zero for PC systems
	 * bit4:   1 = must be 1 for ICW1 (初期化コマンド)
	 * bit3:   0 = edge triggered mode (8259Aの標準モードであるため)
	 * bit2:   0 = 8 byte interrupt vectors (デフォルト設定)
	 * bit1:   0 = cascading 8259's (スレーブあり)
	 * bit0:   1 = ICW4 is needed
	 */
	outb(PIC_SLAVE_CMD, 0x11);
	/** ICW2: IRQ番号とInterrupt Vector番号を対応づける 0x20 = 0010 0000
	 * bit7-3: 0x04 = A7-A3 of interrupt vector
	 * bit2-0: 0 = must be 000 on 80x86 systems
	 * 結果: (IRQ0, INT 0x20), (IRQ1, INT 0x21), ..., (IRQ7, INT 0x27)
	 */
	outb(PIC_SLAVE_IMR, IRQ8_VECTOR);
	/** ICW3: スレーブPICがマスタPICの何番のIRQラインに接続するかを設定する 0x02 = 0000 0010
	 * bit7-3: 0 = no slave
	 * bit2-0: 2 = マスターPICのIRQ2にスレーブPICが接続する
	 * @note スレーブPICでは，接続先のマスターPICのIRQライン番号を数値で指定する
	 */
	outb(PIC_SLAVE_IMR, PIC_CASCADE_IR);
	/** ICW4: 動作モードを設定する 0x01 = 0000 0001
	 * bit7-5: 0  = unused (set to zero)
	 * bit4:   0  = Fully Nested Mode
	 * bit3-2: 00 = not buffered
	 * bit1:   0  = EOI (End Of Interrupt) を手動で送信
	 * bit0:   1  = 80x86 mode
	 */
	outb(PIC_SLAVE_IMR, SLAVE_ICW4_DEFAULT);
}

/** ハードウェア割り込みハンドラをIDTに登録する
 * @note PIC初期化後、割り込み有効化前に呼び出す
 */
void init_IRQ(void)
{
	/* IRQハンドラを登録 */
	set_intr_gate(0x20, irq0);	/* IRQ0: Timer */
	set_intr_gate(0x21, irq1);	/* IRQ1: Keyboard */
	set_intr_gate(0x22, irq2);	/* IRQ2: Cascade */
	set_intr_gate(0x23, irq3);	/* IRQ3: COM2 */
	set_intr_gate(0x24, irq4);	/* IRQ4: COM1 */
	set_intr_gate(0x25, irq5);	/* IRQ5: LPT2 */
	set_intr_gate(0x26, irq6);	/* IRQ6: Floppy */
	set_intr_gate(0x27, irq7);	/* IRQ7: LPT1 */
	set_intr_gate(0x28, irq8);	/* IRQ8: RTC */
	set_intr_gate(0x29, irq9);	/* IRQ9: ACPI */
	set_intr_gate(0x2A, irq10); /* IRQ10: Available */
	set_intr_gate(0x2B, irq11); /* IRQ11: Available */
	set_intr_gate(0x2C, irq12); /* IRQ12: PS/2 Mouse */
	set_intr_gate(0x2D, irq13); /* IRQ13: FPU */
	set_intr_gate(0x2E, irq14); /* IRQ14: Primary ATA */
	set_intr_gate(0x2F, irq15); /* IRQ15: Secondary ATA */
}
