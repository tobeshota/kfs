#ifndef _KFS_UNISTD_H
#define _KFS_UNISTD_H

#include <kfs/pid.h> /* pid_t */

/* POSIX プロセス管理 API（lib/unistd.c で実装） */
pid_t fork(void);
void __attribute__((noreturn)) exit(int status);
pid_t wait(int *wstatus);
int write(int fd, const void *buf, unsigned int count);
int msleep(unsigned int ms);

#endif /* _KFS_UNISTD_H */
