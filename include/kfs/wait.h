#ifndef _KFS_WAIT_H
#define _KFS_WAIT_H

#include <kfs/sched.h>

/* 子が正常終了した場合に真 */
#define WIFEXITED(status) (((status) & 0x7f) == 0)
/* 正常終了時の終了コード（exit(N) の N）を取り出す */
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
/* シグナルで終了した場合に真 */
#define WIFSIGNALED(status) (((status) & 0x7f) != 0x7f && ((status) & 0x7f) != 0)
/* 終了シグナル番号を取り出す */
#define WTERMSIG(status) ((status) & 0x7f)

/* wait options */
#define WNOHANG 1 /* ゾンビ子がいなくても即返り */

pid_t do_wait(int *wstatus, int options);
pid_t sys_wait(int *wstatus);

#endif /* _KFS_WAIT_H */
