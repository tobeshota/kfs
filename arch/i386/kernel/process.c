#include <asm-i386/desc.h>
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
 * @param user_eip ユーザープロセスの開始アドレス
 * @param user_esp ユーザープロセスのスタックポインタ
 *
 * @brief
 * fork() で生成された子プロセスが初めて __switch_to() でスケジュールされた際に，
 * ret_from_fork から実行を開始できるよう fork_frame を構築する．
 *
 * @note callee-saved レジスタの初期値は 0（子プロセスは親の値を継承しない）
 */
void copy_thread(struct task_struct *p, struct task_struct *orig, unsigned long user_eip, unsigned long user_esp)
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
	if (user_eip)
	{
		/* ring-0(カーネル空間)からdo_fork()等で
		 * ring-3で動くユーザプロセスを生成する場合 */
		memset(childregs, 0, sizeof(*childregs));
		childregs->eip = user_eip;
		childregs->cs = __USER_CS | 3;
		childregs->esp = user_esp;
		childregs->ss = __USER_DS | 3;
		childregs->eflags = 0x200; /* IF=1 */
		/* RESTORE_ALL で popl %ds 等が走るため 0 のままだと
		 * ring-3 復帰直後に DS=0 → メモリアクセス例外が発生する */
		childregs->ds = __USER_DS | 3;
		childregs->es = __USER_DS | 3;
		childregs->fs = __USER_DS | 3;
		childregs->gs = __USER_DS | 3;
	}
	else
	{
		/* ring-3(ユーザ空間)からINT 0x80等で
		 * ring-3で動くユーザプロセスを生成する場合 */
		memset(childregs, 0, sizeof(*childregs));
		if (orig)
		{
			/* 親の pt_regs をコピーすることで子の cs/eip/esp/ss も
			 * 親と同じ ring-3 値になり，ret_from_fork → iret で
			 * 自動的に ring-3 に戻る */
			*childregs = *task_pt_regs(orig);
		}
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

/* __switch_to() の実装は arch/i386/kernel/entry.S で行う */
