#ifndef _TEST_RUN_IN_RING3_H
#define _TEST_RUN_IN_RING3_H

#include <kfs/sched.h>	/* do_fork() */
#include <kfs/stdint.h> /* pid_t */
#include <kfs/wait.h>	/* do_wait() */

/** ring-3 で fn() を実行するテスト用ユーティリティ
 * @brief
 * 役割: 「ring-0 テストコードを ring-3 に降ろすための踏み台」のみ。
 * INT 0x80 を迂回するラッパーではない。
 *
 * fn() 内部では fork()/exec_fn()/wait()/exit() など
 * lib/unistd.c の本物の INT 0x80 実装を自由に呼べる。
 *
 * @param fn       ring-3 で実行する関数（引数なし、exit() で終了すること）
 * @param stack    スタック用バッファ（静的配列 or kmalloc 済みのもの）
 * @param stack_sz stack の要素数（unsigned long 単位）
 */
static inline void run_in_ring3(void (*fn)(void), unsigned long *stack, int stack_sz)
{
	unsigned long stack_top = (unsigned long)(stack + stack_sz);
	/* 子を ring-3 で起動: copy_thread が cs/ss/eip/esp を ring-3 用に設定 */
	do_fork((unsigned long)fn, stack_top);
	/* 親: fn() が exit() を呼ぶまで待つ */
	do_wait(NULL);
}

#endif /* _TEST_RUN_IN_RING3_H */
