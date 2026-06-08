#ifndef _KFS_WAIT_H
#define _KFS_WAIT_H

#include <kfs/sched.h>

/* wait status のエンコードに使う内部定数 */
#define __WSTATUS_EVENT_MASK 0xff  /* イベントマスク */
#define __WSTATUS_STOP_TAG 0x7f	   /* 停止イベントのタグ（下位8bit） */
#define __WSTATUS_CONTINUED 0xffff /* 再開イベントのステータス */

/* 停止通知用 status を構築する（上位8bit=停止シグナル, 下位8bit=STOPタグ） */
#define W_STOPCODE(sig) ((((sig) & __WSTATUS_EVENT_MASK) << 8) | __WSTATUS_STOP_TAG)

/* 子が正常終了した場合に真 */
#define WIFEXITED(status) (((status) & __WSTATUS_STOP_TAG) == 0)
/* 正常終了時の終了コード（exit(N) の N）を取り出す */
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
/* シグナルで終了した場合に真 */
#define WIFSIGNALED(status)                                                                                            \
	(((status) & __WSTATUS_STOP_TAG) != __WSTATUS_STOP_TAG && ((status) & __WSTATUS_STOP_TAG) != 0)
/* 終了シグナル番号を取り出す */
#define WTERMSIG(status) ((status) & __WSTATUS_STOP_TAG)
/* 停止した場合に真 */
#define WIFSTOPPED(status) (((status) & __WSTATUS_EVENT_MASK) == __WSTATUS_STOP_TAG)
/* 停止シグナル番号を取り出す */
#define WSTOPSIG(status) (((status) >> 8) & __WSTATUS_EVENT_MASK)
/* 再開通知を受けた場合に真 */
#define WIFCONTINUED(status) ((status) == __WSTATUS_CONTINUED)

/* wait options */
#define WNOHANG 1	 /* ゾンビ子がいなくても即返り */
#define WUNTRACED 2	 /* 停止した子も通知対象にする */
#define WCONTINUED 4 /* 再開した子も通知対象にする */

pid_t do_wait(int *wstatus, int options);
pid_t sys_wait(int *wstatus);
pid_t do_waitpid(pid_t pid, int *wstatus, int options);
pid_t sys_waitpid(pid_t pid, int *wstatus, int options);

#endif /* _KFS_WAIT_H */
