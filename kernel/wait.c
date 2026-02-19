#include <kfs/errno.h>
#include <kfs/list.h>
#include <kfs/sched.h>
#include <kfs/wait.h>

/** 外部参照（kernel/sched/core.c） */
extern struct task_struct *current;

/** 外部参照（kernel/exit.c） */
extern void release_task(struct task_struct *p);

/** 子プロセスの終了を待ち、ゾンビを回収する
 * @param wstatus 終了ステータスを書き込むポインタ（NULLで無視）
 * @return 回収した子プロセスのPID
 *         -ECHILD: 子プロセスが存在しない
 *         -EAGAIN: ゾンビ子がいない（スケジューラ未実装のためブロック不可）
 * @note Linux 6.18 kernel/exit.c do_wait()相当
 * @note Phase 7でschedule()実装後、-EAGAINの代わりにブロックする予定
 */
pid_t do_wait(int *wstatus)
{
	struct task_struct *tsk = current;
	struct list_head *pos, *tmp;
	struct task_struct *child;

	/* 子プロセスがいない場合 */
	if (list_empty(&tsk->children))
	{
		return -ECHILD;
	}

	/* EXIT_ZOMBIE（ゾンビ）状態の子プロセスを検索 */
	list_for_each_safe(pos, tmp, &tsk->children)
	{
		child = list_entry(pos, struct task_struct, sibling);

		if (child->exit_state == EXIT_ZOMBIE)
		{
			pid_t pid = child->pid;

			/* 終了ステータスを返す */
			if (wstatus)
			{
				*wstatus = child->exit_code;
			}

			/* 我が子を揮発させる */
			release_task(child);

			return pid;
		}
	}

	/* ゾンビ子がいない
	 * Phase 7でschedule()実装後、ここでブロックしてゾンビを待つ予定 */
	return -EAGAIN;
}

/** waitシステムコール用ヘルパー
 * @param wstatus 終了ステータスを書き込むポインタ
 * @return 回収した子プロセスのPID、エラー時負数
 * @note Linux 6.18 kernel/exit.c sys_wait4()相当（最小実装）
 * @note Phase 10でシステムコールテーブルから呼ばれる
 */
pid_t sys_wait(int *wstatus)
{
	return do_wait(wstatus);
}
