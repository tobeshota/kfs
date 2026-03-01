#ifndef _KFS_UNISTD_H
#define _KFS_UNISTD_H

#include <kfs/pid.h> /* pid_t */

/* POSIX プロセス管理 API（lib/unistd.c で実装） */
pid_t fork(void);
void __attribute__((noreturn)) exit(int status);
pid_t wait(int *wstatus);
int write(int fd, const void *buf, unsigned int count);
int msleep(unsigned int ms);
int psg_note(int ch, unsigned int freq_hz, unsigned int deadline_ms);
int psg_stop(int ch);
void sigreturn(void); /* シグナルハンドラ return 後に sys_sigreturn syscall を発行して元コンテキストへ復帰 */

#endif /* _KFS_UNISTD_H */
