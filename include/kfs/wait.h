#ifndef _KFS_WAIT_H
#define _KFS_WAIT_H

#include <kfs/sched.h>

/** 子プロセスの終了を待ち、ゾンビを回収する
 * @param wstatus 終了ステータスを書き込むポインタ（NULLで無視）
 * @return 回収した子プロセスのPID
 * @note Linux 6.18 kernel/exit.c do_wait()相当
 */
pid_t do_wait(int *wstatus);

/** waitシステムコール用ヘルパー
 * @param wstatus 終了ステータスを書き込むポインタ
 * @return 回収した子プロセスのPID、エラー時負数
 * @note Linux 6.18 kernel/exit.c sys_wait4()相当（最小実装）
 * @note Phase 10でシステムコールテーブルから呼ばれる
 */
pid_t sys_wait(int *wstatus);

#endif /* _KFS_WAIT_H */
