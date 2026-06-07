#ifndef _KFS_SYSCALL_H
#define _KFS_SYSCALL_H

/* システムコール番号 */
#define __NR_exit 1
#define __NR_fork 2
#define __NR_read 3
#define __NR_write 4
#define __NR_wait 7					/* Linux i386 互換番号 */
#define __NR_getuid 24				/* Linux i386 互換番号 */
#define __NR_kill 37				/* Linux i386 互換番号 */
#define __NR_signal 48				/* Linux i386 互換番号 */
#define __NR_setsid 66				/* Linux i386 互換番号 */
#define __NR_sched_setscheduler 144 /* Linux i386 互換番号 */
#define __NR_sched_getscheduler 145 /* Linux i386 互換番号 */
#define __NR_prctl 157
#define __NR_msleep 158
#define __NR_psg_note 159
#define __NR_psg_stop 160
#define __NR_ps_snapshot 161
#define __NR_sigreturn 119 /* Linux i386 互換番号 */
#define __NR_munmap 91	   /* Linux i386 互換番号 */
#define __NR_mmap2 192	   /* Linux i386 互換番号（引数6本をレジスタで渡す版） */
#define __NR_waitpid 247
#define __NR_setpgid 248
#define __NR_getpgid 249
#define __NR_getpgrp 250
#define __NR_tcgetpgrp 251
#define __NR_tcsetpgrp 252
#define __NR_reboot 253
#define __NR_halt 254
#define __NR_kbd_set_layout 255
#define __NR_panic 256
#define __NR_kbd_read_event 257
#define __NR_kbd_clear_events 258
#define __NR_kbd_set_raw_mode 259
#define __NR_neofetch_info 260
#define __NR_ttynr 261
#define __NR_openpty 262

/* サポートするシステムコールの数 */
#define NR_syscalls 263

/* システムコールのエントリポイント(entry.Sで定義) */
extern void system_call(void);

/* システムコールディスパッチャ */
long do_syscall(long nr, long arg1, long arg2, long arg3, long arg4, long arg5);

#endif /* _KFS_SYSCALL_H */
