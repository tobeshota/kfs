#include <asm-i386/pgtable.h>
#include <kfs/mm.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>

/** 外部参照（kernel/sched/core.c） */
extern struct task_struct *current;
extern struct task_struct init_task;
extern struct list_head task_list;

/** プロセスを終了させる
 * @param code 終了コード（親プロセスに返される値）
 * @note Linux 6.18 kernel/exit.c do_exit()相当
 * @note この関数は返ってこない（スケジューラに制御を渡す）
 */
void do_exit(int code)
{
	struct task_struct *tsk = current;

	/* 終了コードを設定（親がwaitで取得する） */
	tsk->exit_code = code;

	/* 終了中フラグを設定 */
	tsk->flags |= PF_EXITING;

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

	/* 子プロセスの親をinit_task（PID=0）に変更 */
	if (!list_empty(&tsk->children))
	{
		struct list_head *pos, *tmp;

		list_for_each_safe(pos, tmp, &tsk->children)
		{
			struct task_struct *child = list_entry(pos, struct task_struct, sibling);

			/* 子プロセスの親をinit_taskに変更 */
			child->parent = &init_task;

			/* 元の親の子リストから削除 */
			list_del(&child->sibling);

			/* init_taskの子リストに追加 */
			list_add_tail(&child->sibling, &init_task.children);
		}
	}

	/* ゾンビ状態に遷移（親がwait()で回収するまで） */
	tsk->exit_state = EXIT_ZOMBIE;

	/* TASK_DEADに変更（スケジューラがrunqueueから除外する） */
	tsk->__state = TASK_DEAD;

	/* TODO: Phase 4でSIGCHLDを親に送信 */
	/* send_signal(SIGCHLD, tsk->parent); */

	/* スケジューラに制御を渡す（この関数は返ってこない） */
	/* TODO: Phase 7でschedule()実装 */
	/* schedule(); */
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

	/* 親の子リストから削除 */
	list_del(&p->sibling);

	/* PIDを解放 */
	/* TODO: Phase 1でPID管理を完全実装後、正しいpid構造体を取得してput_pid()呼び出し */
	/* struct pid *pid = ...; */
	/* put_pid(pid); */

	/* シグナル構造体を解放 */
	if (p->signal)
	{
		kfree(p->signal);
		p->signal = NULL;
	}

	/* スタックを解放 */
	if (p->stack)
	{
		kfree(p->stack);
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
 * @note Phase 10でシステムコールテーブルから呼ばれる
 */
void sys_exit(int error_code)
{
	do_exit((error_code & 0xff) << 8); /* POSIX形式: 終了コードを上位8ビットに格納 */
}
