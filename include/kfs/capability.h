#ifndef _KFS_CAPABILITY_H
#define _KFS_CAPABILITY_H

#include <kfs/stdint.h>

/** POSIX Capability型
 * @brief 64ビットのCapabilityビットマスク（u32 x 2）
 * @note Linux 6.18の kernel_cap_t と同形式
 */
typedef struct
{
	uint32_t cap[2]; /* Capabilityビットマスク（64ビット = u32 x 2） */
} kernel_cap_t;

/* POSIX Capability定数 */
#define CAP_CHOWN 0		   /* ファイル所有者変更 */
#define CAP_DAC_OVERRIDE 1 /* DAC（任意アクセス制御）を無視 */
#define CAP_KILL 5		   /* 任意プロセスへシグナル送信 */
#define CAP_SETUID 7	   /* UID設定 */
#define CAP_SETPCAP 8	   /* Capability の移譲・削除 */
#define CAP_SYS_ADMIN 21   /* システム管理操作 */
#define CAP_SYS_NICE 23	   /* nice/setpriority/スケジューリングポリシー変更 */

/* 全Capability有効 */
#define CAP_FULL_SET ((kernel_cap_t){{0xffffffff, 0xffffffff}})
/* 全Capability無効 */
#define CAP_EMPTY_SET ((kernel_cap_t){{0, 0}})

#define cap_raise(c, flag) ((c).cap[(flag) >> 5] |= (1u << ((flag) & 31)))	/* flagを有効化する */
#define cap_lower(c, flag) ((c).cap[(flag) >> 5] &= ~(1u << ((flag) & 31))) /* flagを無効化する */
#define cap_raised(c, flag) ((c).cap[(flag) >> 5] & (1u << ((flag) & 31))) /* flagが有効なら0以外の値を返す */

/* cap_capget/cap_capset は sched.h に依存するため前方宣言で参照する */
struct task_struct;

int cap_capget(struct task_struct *tsk, kernel_cap_t *effective, kernel_cap_t *permitted, kernel_cap_t *inheritable);
int cap_capset(struct task_struct *tsk, const kernel_cap_t *effective, const kernel_cap_t *permitted,
			   const kernel_cap_t *inheritable);

#endif /* _KFS_CAPABILITY_H */
