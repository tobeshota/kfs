#include <asm-i386/pgtable.h>
#include <asm-i386/ptrace.h>
#include <kfs/mm_types.h>
#include <kfs/sched.h>
#include <kfs/stddef.h>
#include <kfs/string.h>

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
	struct pt_regs *childregs;
	struct fork_frame *frame;

	/** スタック最上部に pt_regs を配置し、その直下に fork_frame を積む
	 *
	 * レイアウト（高アドレス→低アドレス）:
	 *   stack + THREAD_SIZE
	 *   ┌──────────────┐
	 *   │  pt_regs     │ ← childregs = task_pt_regs(p)
	 *   ├──────────────┤
	 *   │  fork_frame  │ ← frame = (struct fork_frame *)childregs - 1
	 *   └──────────────┘ ← thread.sp
	 */
	childregs = task_pt_regs(p);
	memset(childregs, 0, sizeof(*childregs));
	if (orig)
	{
		/* 親の pt_regs をコピーして子の eax だけ 0 に書き換える */
		*childregs = *task_pt_regs(orig);
	}
	childregs->eax = 0; /* 子の fork() 戻り値 */

	frame = (struct fork_frame *)childregs - 1;
	frame->edi = 0;
	frame->esi = 0;
	frame->ebx = 0;
	frame->ebp = 0;
	frame->ret_addr = (unsigned long)ret_from_fork;

	p->thread.sp = (unsigned long)frame;
	p->thread.ip = (unsigned long)ret_from_fork;
}

/** exec_fn() 専用: カーネルスレッドとして fn を実行するフレームを構築する
 * @brief copy_thread() との違いは fork_frame.ebx に fn を仕込む点のみ．
 * @param p  新しい子プロセス
 * @param fn fork 後に ret_from_fork が call する関数ポインタ
 * @note ret_from_fork は ebx != 0 の場合に call *%%ebx を実行する
 * @see  ret_from_fork (arch/i386/kernel/entry.S)
 */
void copy_thread_with_fn(struct task_struct *p, void (*fn)(void))
{
	extern void ret_from_fork(void); /* entry.S で定義 */
	struct pt_regs *childregs;
	struct fork_frame *frame;

	/* copy_thread() と同じレイアウト: pt_regs at top, fork_frame below */
	childregs = task_pt_regs(p);
	memset(childregs, 0, sizeof(*childregs));

	frame = (struct fork_frame *)childregs - 1;
	frame->edi = 0;
	frame->esi = 0;
	frame->ebx = (unsigned long)fn; /* ret_from_fork が call *%%ebx で呼び出す */
	frame->ebp = 0;
	frame->ret_addr = (unsigned long)ret_from_fork;

	p->thread.sp = (unsigned long)frame;
	p->thread.ip = (unsigned long)ret_from_fork;
}

/* __switch_to() の実装は arch/i386/kernel/entry.S で行う */
