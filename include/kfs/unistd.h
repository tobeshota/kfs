#ifndef _KFS_UNISTD_H
#define _KFS_UNISTD_H

#include <kfs/pid.h> /* pid_t */

/* POSIX プロセス管理 API（lib/unistd.c で実装） */
pid_t fork(void);
void __attribute__((noreturn)) exit(int status);
pid_t wait(int *wstatus);
int write(int fd, const void *buf, unsigned int count);
int prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
int msleep(unsigned int ms);
int psg_note(int ch, unsigned int freq_hz, unsigned int deadline_ms);
int psg_stop(int ch);
void sigreturn(void);
void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff);
int munmap(void *addr, unsigned long len);

#endif /* _KFS_UNISTD_H */
