#ifndef _KFS_SYS_H
#define _KFS_SYS_H

#include <kfs/capability.h>
#include <kfs/keyboard.h>
#include <kfs/neofetch.h>
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
int sys_neofetch_info(struct kfs_neofetch_info *info);
long sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
long sys_msleep(uint32_t ms);
long sys_write(int fd, const char *buf, size_t count);
int sys_setpgid(pid_t pid, pid_t pgid);
pid_t sys_getpgid(pid_t pid);
pid_t sys_getpgrp(void);
pid_t sys_setsid(void);
pid_t sys_tcgetpgrp(int fd);
int sys_tcsetpgrp(int fd, pid_t pgrp);
int sys_ttynr(void);
int sys_kbd_set_layout(kbd_layout_t layout);
long sys_kbd_read_event(struct kfs_keyboard_raw_event *event);
int sys_kbd_clear_events(void);
int sys_kbd_set_raw_mode(int enabled);
void sys_panic(void) __attribute__((noreturn));

#endif /* _KFS_SYS_H */
