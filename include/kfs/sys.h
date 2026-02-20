#ifndef _KFS_SYS_H
#define _KFS_SYS_H

#include <kfs/capability.h>
#include <kfs/sched.h>

int sys_getuid(void);
int sys_setuid(uid_t uid);
int sys_capget(pid_t pid, kernel_cap_t *effective, kernel_cap_t *permitted, kernel_cap_t *inheritable);
int sys_capset(pid_t pid, const kernel_cap_t *effective, const kernel_cap_t *permitted,
			   const kernel_cap_t *inheritable);
int sys_sched_setscheduler(pid_t pid, int policy, int priority);
int sys_sched_getscheduler(pid_t pid);

#endif /* _KFS_SYS_H */
