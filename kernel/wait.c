#include <kfs/errno.h>
#include <kfs/list.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/wait.h>

/** 外部参照（kernel/sched/core.c） */
extern struct task_struct *current;

/** 外部参照（kernel/exit.c） */
extern void release_task(struct task_struct *p);

/** 子プロセスの終了を待ち，終了した子プロセスを揮発させる
 * @brief ゾンビプロセスとなった子プロセスを探し揮発させる．
 * @param wstatus 終了ステータスを書き込むポインタ（NULLで無視）
 * @return 回収した子プロセスのPID
 *         -ECHILD: 子プロセスが存在しない
 *         -EAGAIN: ゾンビ子なし＋schedule()がスイッチしなかった（runnable な子なし）
 * @note Linux 6.18 kernel/exit.c do_wait()相当
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

	/* ゾンビ子が現れるまで schedule() でCPUを譲り続ける */
	while (1)
	{
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

		/* ゾンビ子がまだいない: 子プロセスが実行できるよう CPU を譲る.
		 * schedule() が0を返した（runnable な子が存在しない）
		 * 場合は -EAGAIN を返して呼び出し元に判断を委ねる。 */
		if (!schedule())
		{
			return -EAGAIN;
		}
	}
}

/** waitシステムコール用ヘルパー
 * @param wstatus 終了ステータスを書き込むポインタ
 * @return 回収した子プロセスのPID、エラー時負数
 * @note Linux 6.18 kernel/exit.c sys_wait4()相当（最小実装）
 */
pid_t sys_wait(int *wstatus)
{
	return do_wait(wstatus);
}
