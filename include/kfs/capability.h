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
#define CAP_CHOWN        0  /* ファイル所有者変更 */
#define CAP_DAC_OVERRIDE 1  /* DAC（任意アクセス制御）を無視 */
#define CAP_KILL         5  /* 任意プロセスへシグナル送信 */
#define CAP_SETUID       7  /* UID設定 */
#define CAP_SYS_ADMIN    21 /* システム管理操作 */

/* 全Capability有効 */
#define CAP_FULL_SET  ((kernel_cap_t){{0xffffffff, 0xffffffff}})
/* 全Capability無効 */
#define CAP_EMPTY_SET ((kernel_cap_t){{0, 0}})

#endif /* _KFS_CAPABILITY_H */
