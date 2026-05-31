#ifndef _KFS_SYS_H
#define _KFS_SYS_H

#include <kfs/capability.h>
#include <kfs/ps.h>
#include <kfs/sched.h>

int sys_getuid(void);
int sys_setuid(uid_t uid);
int sys_capget(pid_t pid, kernel_cap_t *effective, kernel_cap_t *permitted, kernel_cap_t *inheritable);
int sys_capset(pid_t pid, const kernel_cap_t *effective, const kernel_cap_t *permitted,
			   const kernel_cap_t *inheritable);
int sys_sched_setscheduler(pid_t pid, int policy, int priority);
int sys_sched_getscheduler(pid_t pid);
long sys_ps_snapshot(struct kfs_ps_entry *entries, size_t max_entries);
long sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
long sys_msleep(uint32_t ms);
long sys_write(int fd, const char *buf, size_t count);

#endif /* _KFS_SYS_H */
