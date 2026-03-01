#ifndef _KFS_SYSCALL_H
#define _KFS_SYSCALL_H

/* システムコール番号 */
#define __NR_exit 1
#define __NR_fork 2
#define __NR_write 4
#define __NR_wait 7					/* Linux i386 互換番号 */
#define __NR_getuid 24				/* Linux i386 互換番号 */
#define __NR_kill 37				/* Linux i386 互換番号 */
#define __NR_signal 48				/* Linux i386 互換番号 */
#define __NR_sched_setscheduler 156 /* Linux i386 互換番号 */
#define __NR_sched_getscheduler 157 /* Linux i386 互換番号 */
#define __NR_msleep 158
#define __NR_psg_note 159
#define __NR_psg_stop 160
#define __NR_sigreturn 119 /* Linux i386 互換番号 */
#define __NR_munmap 91	   /* Linux i386 互換番号 */
#define __NR_mmap2 192	   /* Linux i386 互換番号（引数6本をレジスタで渡す版） */

/* サポートするシステムコールの数 */
#define NR_syscalls 193

/* システムコールのエントリポイント(entry.Sで定義) */
extern void system_call(void);

/* システムコールディスパッチャ */
long do_syscall(long nr, long arg1, long arg2, long arg3, long arg4, long arg5);

#endif /* _KFS_SYSCALL_H */
