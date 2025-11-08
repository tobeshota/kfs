#include <kfs/printk.h>
#include <kfs/stdint.h>

/* boot.Sで定義されたスタック領域の境界。スタックのオーバーフロー検出に必要 */
extern char stack_bottom[];
extern char stack_top[];

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
	printk("Stack dump (sp=%p, bottom=%p, top=%p)\n", sp, stack_bottom, stack_top);

	/* スタックの生の値を16進数でダンプ（最大32ワード） */
	int words = 0;
	for (unsigned long *p = sp; p < (unsigned long *)stack_top && words < 32; ++p, ++words)
	{
		if (!in_stack_bounds(p))
		{
			break;
		}
		printk("  [%p] %08x\n", p, (unsigned int)*p);
	}

	/* EBP(ベースポインタ)を辿って関数呼び出し履歴を追跡する */
	unsigned long *bp;
	asm volatile("mov %%ebp, %0" : "=r"(bp)); /* 現在のベースポインタ(EBP)を取得 */
	if (!in_stack_bounds(bp))
	{
		printk("Base pointer (%p) outside stack bounds, abort frame walk.\n", bp);
		return;
	}
	printk("Call trace (frame walk):\n");
	/* 最大16階層まで関数呼び出しの履歴を遡る */
	for (int depth = 0; depth < 16; ++depth)
	{
		if (!in_stack_bounds(bp) || !in_stack_bounds(bp + 1))
		{
			break;
		}
		unsigned long ret = bp[1]; /* リターンアドレス（呼び出し元のアドレス） */
		printk("  #%d return %p (bp=%p)\n", depth, (void *)ret, (void *)bp);
		unsigned long *next = (unsigned long *)bp[0]; /* 前のベースポインタ */
		if (next <= bp) /* 無限ループを防止（不正なベースポインタ連鎖の検出） */
		{
			break;
		}
		bp = next;
	}
}

/* 現在のスタックをダンプする */
void dump_stack(void)
{
	show_stack(NULL);
}
