/**
 * @file sys.c
 * @brief UID・Capability のシステムコールヘルパー
 */

#include <kfs/capability.h>
#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/sys.h>
#include <kfs/timer.h>

/** 現在のプロセスの実ユーザー ID を返す */
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
