#ifndef _ASM_I386_I8259_H
#define _ASM_I386_I8259_H

/*
 * 8259A PIC I/O port addresses
 * マスター/スレーブPICの制御用ポート
 */
#define PIC_MASTER_CMD 0x20 /* マスターPIC コマンドポート */
#define PIC_MASTER_IMR 0x21 /* マスターPIC データポート(IMR) */
#define PIC_SLAVE_CMD 0xA0	/* スレーブPIC コマンドポート */
#define PIC_SLAVE_IMR 0xA1	/* スレーブPIC データポート(IMR) */

/*
 * 8259A PIC command values
 * 初期化コマンドワード(ICW)とオペレーションコマンドワード(OCW)
 */
#define PIC_CASCADE_IR 2 /* マスターPIC/スレーブPIC間のカスケード接続に使われるIRQライン番号 */
#define MASTER_ICW4_DEFAULT 0x01 /* ICW4: 8086モード */
#define SLAVE_ICW4_DEFAULT 0x01	 /* ICW4: 8086モード */

/*
 * IRQ vector offsets
 * PIC IRQをCPU割り込みベクタにリマップする際のオフセット
 */
#define IRQ0_VECTOR 0x20 /* IRQ0のベクタ番号 (タイマー) */
#define IRQ8_VECTOR 0x28 /* IRQ8のベクタ番号 (スレーブPIC開始) */

/* IRQ数の定義 */
#define NR_IRQS 16 /* 8259A PICがサポートするIRQ数 */

/*
 * EOI (End Of Interrupt) commands
 * 割り込み終了通知用コマンド
 */
#define PIC_EOI 0x20 /* Non-Specific EOI */

void init_8259A(void);
void mask_and_ack_8259A(unsigned int irq);
void disable_8259A_irq(unsigned int irq);
void enable_8259A_irq(unsigned int irq);

#endif /* _ASM_I386_I8259_H */
