#ifndef _KFS_UNISTD_H
#define _KFS_UNISTD_H

#include <kfs/pid.h> /* pid_t */
#include <kfs/ps.h>
#include <kfs/signal.h>

/* POSIX プロセス管理 API（lib/unistd.c で実装） */
pid_t fork(void);
void __attribute__((noreturn)) exit(int status);
pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
pid_t getpgrp(void);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int write(int fd, const void *buf, unsigned int count);
int kill(pid_t pid, int sig);
int prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
int msleep(unsigned int ms);
int psg_note(int ch, unsigned int freq_hz, unsigned int deadline_ms);
int psg_stop(int ch);
long ps_snapshot(struct kfs_ps_entry *entries, size_t max_entries);
void sigreturn(void);
void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, unsigned long pgoff);
int munmap(void *addr, unsigned long len);

#endif /* _KFS_UNISTD_H */
