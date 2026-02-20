#ifndef _KFS_SYSCALL_H
#define _KFS_SYSCALL_H

/* システムコール番号 */
#define __NR_exit 1
#define __NR_write 4
#define __NR_sched_setscheduler 156 /* Linux i386 互換番号 */
#define __NR_sched_getscheduler 157 /* Linux i386 互換番号 */

/* サポートするシステムコールの数 */
#define NR_syscalls 158

/* システムコールのエントリポイント(entry.Sで定義) */
extern void system_call(void);

/* システムコールディスパッチャ */
long do_syscall(long nr, long arg1, long arg2, long arg3, long arg4, long arg5);

#endif /* _KFS_SYSCALL_H */
