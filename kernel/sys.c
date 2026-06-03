#include <kfs/capability.h>
#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/list.h>
#include <kfs/pid.h>
#include <kfs/prctl.h>
#include <kfs/ps.h>
#include <kfs/sched.h>
#include <kfs/serial.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/timer.h>

/** @brief task_struct の状態から ps 表示用の1文字を返す
 * @param task 対象の task_struct
 * @return 状態文字 ('R','S','D','T','Z','X' など)、不明時は '?'
 */
static char ps_state_char(const struct task_struct *task)
{
	if (task->exit_state == EXIT_ZOMBIE)
	{
		return 'Z';
	}
	if (task->exit_state == EXIT_DEAD)
	{
		return 'X';
	}
	if (task->__state == TASK_RUNNING)
	{
		return 'R';
	}
	if (task->__state & TASK_INTERRUPTIBLE)
	{
		return 'S';
	}
	if (task->__state & TASK_UNINTERRUPTIBLE)
	{
		return 'D';
	}
	if (task->__state & __TASK_STOPPED)
	{
		return 'T';
	}
	return '?';
}

/** @brief ps_snapshot の内部コンテキスト
 * @details コールバックへ渡され、書き込み先配列・上限・現在の件数を保持する
 * @note ctx は context の略
 */
struct ps_snapshot_ctx
{
	struct kfs_ps_entry *entries;
	size_t max_entries;
	size_t count;
};

/** @brief タスク走査コールバック — 各タスク情報を `kfs_ps_entry` に詰める
 * @param task 現在のタスク
 * @param ctx  `struct ps_snapshot_ctx *` にキャスト可能なコンテキスト
 * @return 0=継続, 正数=走査中断（バッファ満杯等）
 * @note 出力バッファの上限に達すると正値を返して走査を中断する。
 */
static int ps_snapshot_collect(struct task_struct *task, void *ctx)
{
	struct ps_snapshot_ctx *snapshot = (struct ps_snapshot_ctx *)ctx;
	struct kfs_ps_entry *entry;

	if (snapshot->count >= snapshot->max_entries)
	{
		/* バッファが満杯なら走査を中断する（呼び出し元で件数を確認） */
		return 1;
	}

	entry = &snapshot->entries[snapshot->count++];
	entry->pid = task->pid;
	entry->ppid = task->parent ? task->parent->pid : 0;
	entry->tty[0] = '-';
	entry->tty[1] = '\0';
	/* 現在はダミーの TIME 表示。将来 jiffies -> hh:mm:ss 変換を入れる */
	entry->time[0] = '0';
	entry->time[1] = ':';
	entry->time[2] = '0';
	entry->time[3] = '0';
	entry->time[4] = '\0';
	entry->stat[0] = ps_state_char(task);
	entry->stat[1] = '\0';
	strncpy(entry->cmd, task->comm, sizeof(entry->cmd));
	entry->cmd[sizeof(entry->cmd) - 1] = '\0';
	return 0;
}

long sys_ps_snapshot(struct kfs_ps_entry *entries, size_t max_entries)
{
	struct ps_snapshot_ctx ctx;

	if (!entries && max_entries != 0)
	{
		return -EINVAL;
	}

	ctx.entries = entries;
	ctx.max_entries = max_entries;
	ctx.count = 0;

	if (task_for_each(ps_snapshot_collect, &ctx) < 0)
	{
		return -EINVAL;
	}

	return (long)ctx.count;
}

/* 現在のプロセスの実ユーザー ID を返す */
int sys_getuid(void)
{
	return (int)current->uid.val;
}

/** 現在のプロセスの UID を変更する
 * @param uid 設定する UID
 * @return 0: 成功, -EPERM: CAP_SETUID がない
 */
int sys_setuid(uid_t uid)
{
	if (!capable(CAP_SETUID))
	{
		return -EPERM;
	}
	current->uid.val = uid;
	current->euid.val = uid;
	return 0;
}

/** 指定 PID のタスクの Capability セットを取得する
 * @param pid         対象 PID（0の場合呼び出し元プロセス）
 * @param effective   有効 Capability の格納先
 * @param permitted   許可 Capability の格納先（NULL で省略可）
 * @param inheritable 継承 Capability の格納先（NULL で省略可）
 * @return 0: 成功, -ESRCH: PID が見つからない, -EINVAL: 引数不正
 */
int sys_capget(pid_t pid, kernel_cap_t *effective, kernel_cap_t *permitted, kernel_cap_t *inheritable)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}
	return cap_capget(tsk, effective, permitted, inheritable);
}

/** 指定 PID のタスクの有効 Capability セットを設定する
 * @param pid         対象 PID（0の場合呼び出し元プロセス）
 * @param effective   設定する有効 Capability
 * @param permitted   設定する許可 Capability（現在は無視）
 * @param inheritable 設定する継承 Capability（現在は無視）
 * @return 0: 成功, -ESRCH: PID が見つからない, -EINVAL/-EPERM: cap_capset に準じる
 */
int sys_capset(pid_t pid, const kernel_cap_t *effective, const kernel_cap_t *permitted, const kernel_cap_t *inheritable)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}
	return cap_capset(tsk, effective, permitted, inheritable);
}

/** スケジューリングポリシーとリアルタイム優先度を設定する
 * @param pid      対象 PID（0の場合呼び出し元プロセス）
 * @param policy   設定するポリシー（SCHED_*）
 * @param priority RT 優先度（SCHED_FIFO/RR/DEADLINE 用、非 RT ポリシーでは 0 のみ有効）
 * @return 0: 成功, -ESRCH: PID 未存在, -EINVAL: 不正ポリシーまたは非 RT に priority != 0, -EPERM: 権限不足
 */
int sys_sched_setscheduler(pid_t pid, int policy, int priority)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}

	/* 設定するpolicyが有効（SCHED_*）であるか確認 */
	if (policy != SCHED_NORMAL && policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_BATCH &&
		policy != SCHED_IDLE && policy != SCHED_DEADLINE && policy != SCHED_PURE_RR)
	{
		return -EINVAL;
	}

	/* 非 RT ポリシーに priority != 0 は不正（Linux 6.18 準拠） */
	if (priority != 0 && (policy != SCHED_FIFO && policy != SCHED_RR && policy != SCHED_DEADLINE))
	{
		return -EINVAL;
	}

	/* リアルタイム系ポリシー（FIFO/RR/DEADLINE）は CAP_SYS_NICE が必要 */
	if ((policy == SCHED_FIFO || policy == SCHED_RR || policy == SCHED_DEADLINE) && !capable(CAP_SYS_NICE))
	{
		return -EPERM;
	}

	tsk->policy = (unsigned int)policy;
	tsk->rt_priority = priority;
	return 0;
}

/** スケジューリングポリシーを取得する
 * @param pid 対象 PID（0の場合呼び出し元プロセス）
 * @return ポリシー値（SCHED_*）: 成功, -ESRCH: PID 未存在
 */
int sys_sched_getscheduler(pid_t pid)
{
	struct task_struct *tsk = (pid == 0) ? current : find_task_by_pid(pid);

	if (!tsk)
	{
		return -ESRCH;
	}

	return (int)tsk->policy;
}

long sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	(void)arg3;
	(void)arg4;
	(void)arg5;

	if (option == PR_SET_NAME)
	{
		strncpy(current->comm, (const char *)arg2, sizeof(current->comm) - 1);
		current->comm[sizeof(current->comm) - 1] = '\0';
		return 0;
	}
	return -ENOSYS; /* 未実装 */
}

/** ms ミリ秒スリープする
 * @param ms スリープ時間 [ミリ秒]（HZ=1000 なので ms == tick 数）
 * @return 0: 成功
 */
long sys_msleep(uint32_t ms)
{
	if (ms == 0)
	{
		return 0;
	}
	schedule_timeout((long)ms);
	return 0;
}

/** 引数pidで指定されたプロセスのプロセスグループIDをgpidに設定する
 * @param pid 対象 PID（0の場合は current を意味する）
 * @param pgid 設定するプロセスグループID（0の場合は pgid = pid を意味する）
 * @return 成功: 0、PIDが見つからない: -ESRCH、引数不正: -EINVAL
 * @example
 * setpgid(0, 0);  // 自身をプロセスグループPID番のプロセスグループリーダーにする
 *                 // （自身のPGIDを PGID == PID となるように設定する）
 *
 * @note 現時点では最小実装として自分自身の pgrp のみ変更を許可する（pid == current->pid）
 */
int sys_setpgid(pid_t pid, pid_t pgid)
{
	if (pid == 0)
	{
		pid = current->pid;
	}
	if (pgid == 0)
	{
		pgid = pid;
	}

	/* 現時点では最小実装として自分自身の pgrp のみ変更を許可する */
	if (pid != current->pid)
	{
		return -ESRCH;
	}

	struct task_struct *tsk = find_task_by_pid(pid);
	if (!tsk)
	{
		return -ESRCH;
	}

	if (pgid <= 0)
	{
		return -EINVAL;
	}

	tsk->pgrp = pgid;
	return 0;
}

/** 引数pidで指定されたプロセスのプロセスグループIDを返す
 * @param pid 対象 PID（0の場合は current を意味する）
 * @return 成功時: pgrp，PIDが見つからない場合: -ESRCH
 */
pid_t sys_getpgid(pid_t pid)
{
	if (pid == 0)
	{
		return current->pgrp;
	}

	struct task_struct *tsk = find_task_by_pid(pid);
	if (!tsk)
	{
		return -ESRCH;
	}
	return tsk->pgrp;
}

/** 呼び出しプロセスのプロセスグループIDを返す
 * @note getgpid(0) と同じ動作
 */
pid_t sys_getpgrp(void)
{
	return current->pgrp;
}

/** 現在の端末のフォアグラウンドプロセスグループIDを返す
 * @param fd 端末のファイルディスクリプタ
 * @return フォアグラウンドプロセスグループID
 * @note 現在は fd を無視した簡易実装である
 */
pid_t sys_tcgetpgrp(int fd)
{
	(void)fd;
	return foreground_pgrp;
}

/** 現在の端末のフォアグラウンドプロセスグループIDをpgrpに設定する
 * @param fd 端末のファイルディスクリプタ
 * @param pgrp 設定するフォアグラウンドプロセスグループID
 * @return 0: 成功，-EINVAL: 不正なプロセスグループID
 * @note 現在は fd を無視した簡易実装である
 */
int sys_tcsetpgrp(int fd, pid_t pgrp)
{
	(void)fd;
	if (pgrp <= 0)
	{
		return -EINVAL;
	}
	foreground_pgrp = pgrp;
	return 0;
}

/** stdout/stderr への書き込みを VGA + COM1 の両方に tee する
 * @param fd    1=stdout/2=stderr → VGA端末 + COM1 両方、4=COM1 のみ
 * @param buf   書き込むバッファ
 * @param count バイト数
 * @return 書き込んだバイト数、エラー時負数
 */
long sys_write(int fd, const char *buf, size_t count)
{
	size_t i;

	if (fd != 1 && fd != 2 && fd != 4)
	{
		return -EBADF;
	}
	if (!buf || count == 0)
	{
		return 0;
	}
	if (fd == 1 || fd == 2)
	{
		/* stdout/stderr: VGA端末 と COM1 の両方に出力（tee） */
		for (i = 0; i < count; i++)
		{
			terminal_putchar(buf[i]);
		}
		serial_write(buf, count);
	}
	else
	{
		/* fd==4: COM1 のみ */
		serial_write(buf, count);
	}
	return (long)count;
}
