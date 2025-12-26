#include <kfs/printk.h>
#include <kfs/stdint.h>

/* boot.Sで定義されたスタック領域の境界。スタックのオーバーフロー検出に必要 */
extern char stack_bottom[]; /* スタック領域の下限アドレス */
extern char stack_top[];	/* スタック領域の上限アドレス */

/* ポインタpがスタック領域内にあるか確認する。不正なメモリアクセスを防ぐために必要 */
static int in_stack_bounds(const void *p)
{
	return (const char *)p >= stack_bottom && (const char *)p < stack_top;
}

/* スタックの内容を人間が読める形式でダンプする。デバッグ時にスタック状態を確認するために必要 */
void show_stack(unsigned long *esp)
{
	unsigned long *sp = esp;
	/* espがNULLの場合は現在のスタックポインタを取得 */
	if (!sp)
	{
		asm volatile("mov %%esp, %0" : "=r"(sp));
	}

	printk("\n========== Stack Trace ==========\n");
	printk("Stack pointer: %p (bottom=%p, top=%p, size=%d bytes)\n", sp, stack_bottom, stack_top,
		   (int)((char *)stack_top - (char *)stack_bottom));

	/* EBP(ベースポインタ)を辿って関数呼び出し履歴を追跡する */
	unsigned long *bp;
	asm volatile("mov %%ebp, %0" : "=r"(bp)); /* 現在のベースポインタ(EBP)を取得 */

	printk("\n--- Call Trace (most recent first) ---\n");
	if (!in_stack_bounds(bp))
	{
		printk("ERROR: Base pointer %p is outside stack bounds!\n", bp);
	}
	else
	{
		/* 最大16階層まで関数呼び出しの履歴を遡る */
		for (int depth = 0; depth < 16; ++depth)
		{
			if (!in_stack_bounds(bp) || !in_stack_bounds(bp + 1))
			{
				break;
			}
			unsigned long ret = bp[1]; /* リターンアドレス（呼び出し元のアドレス） */
			unsigned long *next = (unsigned long *)bp[0]; /* 前のベースポインタ */

			/* より詳細な情報を表示 */
			printk("  [%2d] ret=%08lx  bp=%p", depth, ret, (void *)bp);
			if (ret >= 0xC0200000 && ret <= 0xC0300000)
			{
				printk(" <kernel code>");
			}
			printk("\n");

			if (next <= bp) /* 無限ループを防止（不正なベースポインタ連鎖の検出） */
			{
				break;
			}
			bp = next;
		}
	}

	/* スタックの生の値を16進数でダンプ（最大32ワード） */
	printk("\n--- Stack Dump (first 32 words) ---\n");
	printk("Address       Value      Possible Interpretation\n");
	int words = 0;
	for (unsigned long *p = sp; p < (unsigned long *)stack_top && words < 32; ++p, ++words)
	{
		if (!in_stack_bounds(p))
		{
			break;
		}
		unsigned int val = (unsigned int)*p;
		printk("[%p] %08x", p, val);

		/* 値の解釈を試みる */
		if (val == 0)
		{
			printk("  (NULL)");
		}
		else if (val >= 0xC0200000 && val <= 0xC0300000)
		{
			printk("  <code>");
		}
		else if (val >= (unsigned int)stack_bottom && val < (unsigned int)stack_top)
		{
			printk("  <stack>");
		}
		else if (val < 0x1000)
		{
			printk("  <small value>");
		}
		printk("\n");
	}
	printk("=================================\n\n");
}

/* 現在のスタックをダンプする */
void dump_stack(void)
{
	show_stack(NULL);
}
