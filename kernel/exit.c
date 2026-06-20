#include <asm-i386/pgtable.h>
#include <kfs/exit.h>
#include <kfs/mm.h>
#include <kfs/mman.h>
#include <kfs/pid.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/slab.h>

/** 外部参照（kernel/sched/core.c） */
extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;

#define MAX_EXIT_HOOKS 8

static exit_hook_t exit_hooks[MAX_EXIT_HOOKS];
static int exit_hook_count;

void register_exit_hook(exit_hook_t hook)
{
	if (!hook || exit_hook_count >= MAX_EXIT_HOOKS)
	{
		return;
	}
	exit_hooks[exit_hook_count++] = hook;
}

void invoke_exit_hooks(struct task_struct *tsk)
{
	for (int i = 0; i < exit_hook_count; i++)
	{
		if (exit_hooks[i])
		{
			exit_hooks[i](tsk);
		}
	}
}

/** プロセスを終了させる
 * @param code 終了コード（親プロセスに返される値）
 * @note Linux 6.18 kernel/exit.c do_exit()相当
 * @note この関数は返ってこない（スケジューラに制御を渡す）
 * @note noreturn: schedule() が返ってきた場合もループして再スケジュールを要求し続ける。
 *       これにより exit() syscall が iret で ring-3 に戻ることを防ぐ。
 */
__attribute__((noreturn)) void do_exit(int code)
{
	struct task_struct *tsk = current;

	/* 終了コードを設定（親がwaitで取得する） */
	tsk->exit_code = code;

	/* デバイス固有の終了後始末はフック経由で実行する */
	invoke_exit_hooks(tsk);

	/* 保留中シグナルキューを解放 */
	signal_flush_pending(tsk);

	/* 終了中フラグを設定 */
	tsk->flags |= PF_EXITING;

	/* do_mmap で確保したユーザスタックを解放（物理ページ＋VMAノード） */
	if (tsk->user_stack_vm_start != 0)
	{
		do_munmap(tsk->user_stack_vm_start, tsk->user_stack_vm_len);
		tsk->user_stack_vm_start = 0;
		tsk->user_stack_vm_len = 0;
	}

	/* メモリ記述子を解放（mm_struct） */
	if (tsk->mm)
	{
		/* 参照カウントを減らす */
		tsk->mm->mm_count.counter--;
		if (tsk->mm->mm_count.counter == 0)
		{
			/* 最後の参照ならページテーブルとmm_structを解放 */
			free_page_tables(tsk->mm->pgd);
			kfree(tsk->mm);
		}
		tsk->mm = NULL;
	}

	/* 親プロセスがすでに死んでいる（EXIT_ZOMBIE / EXIT_DEAD）場合、
	 * PID 1 (kernel_init) に養子として引き渡す。
	 * 親が先に exit() して自分より前にゾンビになっていると、
	 * 親の do_wait() は二度と呼ばれないためゾンビが永久に残る。 */
	if (tsk->parent && (tsk->parent->exit_state == EXIT_ZOMBIE || tsk->parent->exit_state == EXIT_DEAD))
	{
		struct task_struct *reaper = find_task_by_pid(1);
		if (!reaper)
		{
			reaper = &init_task;
		}
		list_del(&tsk->sibling);
		list_add_tail(&tsk->sibling, &reaper->children);
		tsk->parent = reaper;
	}

	/* 子プロセスの親を child_reaper（PID 1、なければ init_task）に変更 */
	if (!list_empty(&tsk->children))
	{
		struct list_head *pos, *tmp;
		struct task_struct *reaper;

		/* PID 1 (kernel_init) が孤児を引き取る．
		 * 存在しない場合，またはテスト環境で PID 1 が終了したタスク自身の場合は init_task に fallback */
		reaper = find_task_by_pid(1);
		if (!reaper || reaper == tsk)
		{
			reaper = &init_task;
		}

		list_for_each_safe(pos, tmp, &tsk->children)
		{
			struct task_struct *child = list_entry(pos, struct task_struct, sibling);

			child->parent = reaper;
			list_del(&child->sibling);
			list_add_tail(&child->sibling, &reaper->children);
		}
	}

	/* ゾンビ状態に遷移（親がwait()で回収するまで） */
	tsk->exit_state = EXIT_ZOMBIE;

	/* 親プロセスに対してSIGCHLD（子プロセスの状態変化）を送信する */
	if (tsk->parent && tsk->parent != tsk && tsk->parent->exit_state != EXIT_DEAD)
	{
		send_signal(SIGCHLD, tsk->parent);
	}

	/* TASK_DEADに変更（スケジューラがrunqueueから除外する） */
	tsk->__state = TASK_DEAD;
	sched_dequeue_task(tsk); /* ランキューから除外して再スケジュールされないようにする */

	/* TASK_DEAD かつ run queue 外なので schedule() からは二度と戻らない。
	 * __builtin_unreachable() でコンパイラに noreturn を伝える。 */
	schedule();
	__builtin_unreachable();
}

/** プロセスを揮発させる
 * @brief ゾンビプロセスをクリーンアップする
 * @param p クリーンアップするtask_struct
 * @note Linux 6.18 kernel/exit.c release_task()相当
 * @note 親がwait()でゾンビを回収した後に呼ばれる
 */
void release_task(struct task_struct *p)
{
	/* 完全終了状態に遷移（これ以降p->exit_stateはEXIT_DEAD） */
	p->exit_state = EXIT_DEAD;

	/* グローバルタスクリストから削除 */
	list_del(&p->tasks);
	extern int nr_threads;
	nr_threads--;

	/* 親の子リストから削除 */
	list_del(&p->sibling);

	/* PID構造体の参照を解放（put_pid が内部で数値ビットをクリアする） */
	if (p->pid_struct)
	{
		put_pid(p->pid_struct);
		p->pid_struct = NULL;
	}

	/* シグナル構造体を解放 */
	signal_flush_pending(p);
	if (p->signal)
	{
		kfree(p->signal);
		p->signal = NULL;
	}

	/* スタックを解放（alloc_pages で確保したので free_pages で解放） */
	if (p->stack)
	{
		free_pages((struct page *)p->stack, 0);
		p->stack = NULL;
	}

	/* task_struct本体を解放 */
	extern struct kmem_cache *task_struct_cachep; /* kernel/fork.c */
	kmem_cache_free(task_struct_cachep, p);
}

/** _exitシステムコール用ヘルパー
 * @param error_code 終了コード
 * @return この関数は返ってこない
 * @note Linux 6.18 kernel/exit.c sys_exit()相当
 * @note システムコールテーブルから呼ばれる
 */
void sys_exit(int error_code)
{
	do_exit((error_code & 0xff) << 8); /* POSIX形式: 終了コードを上位8ビットに格納 */
}
