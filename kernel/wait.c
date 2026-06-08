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
 * @param options WNOHANG を渡すとゾンビ子がいなくても即返り（0 = ブロック）
 * @return 回収した子プロセスのPID
 *         0       : WNOHANG 指定時，子はいるがまだゾンビでない
 *         -ECHILD : 子プロセスが存在しない
 * @note WNOHANG なし（options==0）の場合，ゾンビ子が現れるまで
 *       schedule() で CPU を譲り続ける．
 */
pid_t do_wait(int *wstatus, int options)
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

		/* WNOHANG: ゾンビ子がいなければ即返り */
		if (options & WNOHANG)
		{
			return 0;
		}

		/* ゾンビ子がまだいない: 子プロセスが実行できるよう CPU を譲る。
		 * init_task が cpu_idle_loop() で hlt するため呼び出し元は hlt 不要。 */
		schedule();
	}
}

/** waitシステムコール用ヘルパー
 * @param wstatus 終了ステータスを書き込むポインタ
 * @return 回収した子プロセスのPID、エラー時負数
 * @note Linux 6.18 kernel/exit.c sys_wait4()相当（最小実装）
 */
pid_t sys_wait(int *wstatus)
{
	return do_wait(wstatus, 0);
}

/** pid 指定版の do_wait: 特定 PID の子を待つ
 * @param pid 待ち対象の PID（-1: any child）
 * @param wstatus 終了ステータス書き込みポインタ
 * @param options WNOHANG サポート
 */
pid_t do_waitpid(pid_t pid, int *wstatus, int options)
{
	struct task_struct *tsk = current;
	struct list_head *pos, *tmp;
	struct task_struct *child;

	/* 子プロセスがいない場合 */
	if (list_empty(&tsk->children))
	{
		return -ECHILD;
	}

	while (1)
	{
		int found = 0;
		list_for_each_safe(pos, tmp, &tsk->children)
		{
			child = list_entry(pos, struct task_struct, sibling);

			if (pid != -1 && child->pid != pid)
			{
				continue; /* 対象外 */
			}
			found = 1;

			if (child->exit_state == EXIT_ZOMBIE)
			{
				pid_t found_pid = child->pid;
				if (wstatus)
				{
					*wstatus = child->exit_code;
				}
				release_task(child);
				return found_pid;
			}

			/* 停止中の子プロセスがある場合 */
			if ((options & WUNTRACED) && (child->flags & PF_WAIT_STOP_PENDING))
			{
				if (wstatus)
				{
					/* 停止通知用 status を構築する */
					*wstatus = W_STOPCODE(child->exit_signal);
				}

				/* 停止通知を処理済みにする */
				child->flags &= ~PF_WAIT_STOP_PENDING;

				return child->pid;
			}

			/* 継続中の子プロセスがある場合 */
			if ((options & WCONTINUED) && (child->flags & PF_WAIT_CONT_PENDING))
			{
				if (wstatus)
				{
					/* 再開通知用 status を構築する */
					*wstatus = __WSTATUS_CONTINUED;
				}

				/* 再開通知を処理済みにする */
				child->flags &= ~PF_WAIT_CONT_PENDING;

				return child->pid;
			}
		}

		/* 指定 PID が存在しない場合は ECHILD */
		if (!found)
		{
			return -ECHILD;
		}

		if (options & WNOHANG)
		{
			return 0;
		}

		schedule();
	}
}

pid_t sys_waitpid(pid_t pid, int *wstatus, int options)
{
	return do_waitpid(pid, wstatus, options);
}
