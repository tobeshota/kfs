#include <asm-i386/pgtable.h>
#include <kfs/mm_types.h>
#include <kfs/sched.h>
#include <kfs/stddef.h>

/** fork() で生成された子プロセスのカーネルスタック末尾に配置される初期スタックフレーム
 * copy_thread() が stack + THREAD_SIZE の直下（= スタック末尾）に1つ配置する
 * @see copy_thread()
 */
struct fork_frame
{
	unsigned long edi;		/* callee-saved: 初期値 0 */
	unsigned long esi;		/* callee-saved: 初期値 0 */
	unsigned long ebx;		/* callee-saved: 初期値 0 */
	unsigned long ebp;		/* callee-saved: 初期値 0 */
	unsigned long ret_addr; /* __switch_to の ret が戻る先（ret_from_fork） */
};

/** プロセスのアドレス空間を切り替える（CR3 レジスタを更新）
 * @param prev 切り替え元のメモリディスクリプタ（未使用OK）
 * @param next 切り替え先のメモリディスクリプタ
 *
 * @details
 * プロセス切り替え時に呼ばれ、CR3 レジスタに次プロセスのページディレクトリを
 * ロードすることで仮想アドレス空間を切り替える。
 * カーネルスレッド（mm == NULL）の場合は切り替えを行わない。
 */
void switch_mm(struct mm_struct *prev, struct mm_struct *next)
{
	(void)prev; /* 今回は未使用 */

	/* カーネルスレッドの場合（mm == NULL）は何もしない */
	if (!next || !next->pgd)
	{
		return;
	}

	/* CR3 レジスタに次プロセスのページディレクトリをロード */
	load_cr3((unsigned long)next->pgd);
}

/** 子プロセスのカーネルスタック上に初期スタックフレームを積む
 * @param p    新しい子プロセス
 * @param orig 親プロセス（現在未使用）
 *
 * @brief
 * fork() で生成された子プロセスが初めて __switch_to() でスケジュールされた際に，
 * ret_from_fork から実行を開始できるよう fork_frame を構築する．
 *
 * @note callee-saved レジスタの初期値は 0（子プロセスは親の値を継承しない）
 */
void copy_thread(struct task_struct *p, struct task_struct *orig)
{
	extern void ret_from_fork(void); /* entry.S で定義 */
	struct fork_frame *frame;

	(void)orig; /* 親タスクは今回未使用 */

	/** スタック末尾の直下にフレームを配置
	 * カーネルスタックの末尾アドレスは，
	 * カーネルスタックの先頭アドレス(p->stack)に
	 * カーネルスタックのサイズ(THREAD_SIZE)を足した値である．
	 * カーネルスタックの末尾アドレスからstruct fork_frameのサイズだけ下がり，
	 * frame->{edi, esi, ebx, ebp, ret_addr}を配置する．
	 */
	frame = (struct fork_frame *)((unsigned long)p->stack + THREAD_SIZE) - 1;

	frame->edi = 0;
	frame->esi = 0;
	frame->ebx = 0;
	frame->ebp = 0;
	/** 子プロセスが最初に実行する関数をret_from_fork()に設定する
	 * @brief 子プロセスのリターンアドレスをret_from_fork()に設定する
	 *
	 * @note __switch_to() は次のようなアセンブリコードで
	 *       子プロセスのスタックからレジスタを復元し，
	 *       ret で ret_from_fork() にジャンプする
	 * ```
	 * 	movl  next->thread.sp, %esp   ; ESP ← frame の先頭
	 * 	popl  %edi                    ; edi 復元（= 0）
	 * 	popl  %esi                    ; esi 復元（= 0）
	 * 	popl  %ebx                    ; ebx 復元（= 0）
	 * 	popl  %ebp                    ; ebp 復元（= 0）
	 * 	ret                           ; ret_addr をポップして ret_from_fork へジャンプ
	 * ```
	 */
	frame->ret_addr = (unsigned long)ret_from_fork;

	p->thread.sp = (unsigned long)frame;
	p->thread.ip = (unsigned long)ret_from_fork;
}

/* __switch_to() の実装は arch/i386/kernel/entry.S で行う */
