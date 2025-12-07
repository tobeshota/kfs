#ifndef _KFS_SYSCALL_H
#define _KFS_SYSCALL_H

/* システムコール番号の定義 */
#define __NR_exit 1
#define __NR_write 4

/* サポートするシステムコールの数(現在8個としているが将来的に増やす可能性あり) */
#define NR_syscalls 8

/* システムコールのエントリポイント(entry.Sで定義) */
extern void system_call(void);

/* システムコールディスパッチャ */
long do_syscall(long nr, long arg1, long arg2, long arg3, long arg4, long arg5);

#endif /* _KFS_SYSCALL_H */
