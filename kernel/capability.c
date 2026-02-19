/**
 * @file capability.c
 * @brief Capability 管理の中核実装
 *
 * @note cap_permitted / cap_inheritable は task_struct に未実装のため
 *       cap_capget は CAP_EMPTY_SET を返し，cap_capset は無視する（将来対応）
 */

#include <kfs/capability.h>
#include <kfs/errno.h>
#include <kfs/sched.h>

/** タスクの Capability セットを取得する
 * @brief tskの有効 Capability を effective にコピーする。
 *        permitted と inheritable は未実装で全て空セットを返す。
 * @param tsk         対象タスク
 * @param effective   有効 Capability の格納先
 * @param permitted   許可 Capability の格納先（NULLで省略可）
 * @param inheritable 継承 Capability の格納先（NULLで省略可）
 * @return 0: 成功, -EINVAL: 引数が不正
 */
int cap_capget(struct task_struct *tsk, kernel_cap_t *effective,
               kernel_cap_t *permitted, kernel_cap_t *inheritable)
{
	if (!tsk || !effective)
		return -EINVAL;
	*effective = tsk->cap_effective;
	if (permitted)
		*permitted = CAP_EMPTY_SET;   /* 未実装 */
	if (inheritable)
		*inheritable = CAP_EMPTY_SET; /* 未実装 */
	return 0;
}

/** タスクの有効 Capability セットを設定する
 * @brief tskの有効 Capability を effective に設定する。
 *        permitted と inheritable は未実装で引数も無視する。
 * @param tsk         対象タスク
 * @param effective   設定する有効 Capability
 * @param permitted   設定する許可 Capability（現在は無視）
 * @param inheritable 設定する継承 Capability（現在は無視）
 * @return 0: 成功, -EINVAL: 引数が不正, -EPERM: 権限不足（CAP_SETPCAP が必要）
 */
int cap_capset(struct task_struct *tsk, const kernel_cap_t *effective,
               const kernel_cap_t *permitted, const kernel_cap_t *inheritable)
{
	if (!tsk || !effective)
		return -EINVAL;
	if (!capable(CAP_SETPCAP))
		return -EPERM;
	tsk->cap_effective = *effective;
	(void)permitted;   /* 未実装 */
	(void)inheritable; /* 未実装 */
	return 0;
}
