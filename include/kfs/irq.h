#ifndef _KFS_IRQ_H
#define _KFS_IRQ_H

#include <asm-i386/i8259.h>
#include <asm-i386/ptrace.h>

/** IRQハンドラ関数の型定義
 * @param irq   割り込み番号
 * @param regs  割り込み発生時のレジスタ状態
 * @return      IRQ_HANDLED or IRQ_NONE
 */
typedef int (*irq_handler_t)(int irq, struct pt_regs *regs);

/** IRQハンドラ戻り値 */
#define IRQ_NONE 0	  /* 割り込みを処理しなかった */
#define IRQ_HANDLED 1 /* 割り込みを処理した */

/** IRQアクション構造体
 * @note 登録されたIRQハンドラの情報を保持
 */
struct irqaction
{
	irq_handler_t handler; /* ハンドラ関数 */
	const char *name;	   /* デバイス名 */
	void *dev_id;		   /* デバイス識別子 */
};

/** IRQディスクリプタ構造体
 * @note 各IRQの状態とハンドラを管理
 */
struct irq_desc
{
	struct irqaction *action; /* 登録されたハンドラ */
	unsigned int count;		  /* 割り込み発生回数 */
};

int request_irq(unsigned int irq, irq_handler_t handler, const char *name, void *dev_id);
void free_irq(unsigned int irq, void *dev_id);
struct irq_desc *irq_to_desc(unsigned int irq);
void do_IRQ(struct pt_regs *regs);
void init_IRQ(void);

#endif /* _KFS_IRQ_H */
