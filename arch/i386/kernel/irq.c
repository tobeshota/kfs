#include <asm-i386/i8259.h>
#include <kfs/irq.h>
#include <kfs/printk.h>
#include <kfs/stddef.h>

/* IRQディスクリプタ配列 (IRQ0-15) */
static struct irq_desc irq_desc_array[NR_IRQS];

/* IRQアクション配列 (各IRQにつき1ハンドラ) */
static struct irqaction irq_actions[NR_IRQS];

/** IRQ番号からIRQディスクリプタを取得する
 * @param irq IRQ番号 (0-15)
 * @return    ディスクリプタへのポインタ (範囲外はNULL)
 */
struct irq_desc *irq_to_desc(unsigned int irq)
{
	if (irq >= NR_IRQS)
	{
		return NULL;
	}
	return &irq_desc_array[irq];
}

/** IRQハンドラを登録する
 * @param irq     IRQ番号 (0-15)
 * @param handler ハンドラ関数
 * @param name    デバイス名 (デバッグ用)
 * @param dev_id  デバイス識別子
 * @return 0: 成功, -1: エラー
 */
int request_irq(unsigned int irq, irq_handler_t handler, const char *name, void *dev_id)
{
	struct irq_desc *desc;
	struct irqaction *action;

	/* 引数チェック */
	if (irq >= NR_IRQS || handler == NULL)
	{
		return -1;
	}

	desc = irq_to_desc(irq);
	/* 既にハンドラが登録されている場合はエラー */
	if (desc->action != NULL)
	{
		return -1;
	}

	/* IRQアクションを設定 */
	action = &irq_actions[irq];
	action->handler = handler;
	action->name = name;
	action->dev_id = dev_id;

	/* ディスクリプタにアクションを登録 */
	desc->action = action;

	/* IRQを有効化 */
	enable_8259A_irq(irq);

	return 0;
}

/** IRQハンドラを解除する
 * @param irq     IRQ番号 (0-15)
 * @param dev_id  デバイス識別子 (request_irqで指定したもの)
 */
void free_irq(unsigned int irq, void *dev_id)
{
	struct irq_desc *desc;

	if (irq >= NR_IRQS)
	{
		return;
	}

	desc = irq_to_desc(irq);
	if (desc->action == NULL)
	{
		return;
	}

	/* dev_idが一致する場合のみ解除 */
	if (desc->action->dev_id != dev_id)
	{
		return;
	}

	/* IRQを無効化 */
	disable_8259A_irq(irq);

	/* ハンドラをクリア */
	desc->action->handler = NULL;
	desc->action->name = NULL;
	desc->action->dev_id = NULL;
	desc->action = NULL;
}

/** 共通IRQハンドラ
 * @param regs 割り込み発生時のレジスタ状態
 * @note  entry.Sから呼び出される。orig_eaxにIRQ番号が格納されている
 */
void do_IRQ(struct pt_regs *regs)
{
	unsigned int irq = regs->orig_eax;
	struct irq_desc *desc;
	struct irqaction *action;

	/* IRQ番号の範囲チェック */
	if (irq >= NR_IRQS)
	{
		printk("do_IRQ: invalid IRQ %u\n", irq);
		return;
	}

	desc = irq_to_desc(irq);
	desc->count++;

	action = desc->action;
	if (action != NULL && action->handler != NULL)
	{
		/* 登録されたハンドラを呼び出す */
		action->handler(irq, regs);
	}

	/* PICにEOIを送信して割り込み処理完了を通知 */
	mask_and_ack_8259A(irq);
	/* IRQを再有効化 */
	enable_8259A_irq(irq);
}
