#ifndef _PTY_H
#define _PTY_H

#include <kfs/pid.h>

#define PTY_MAX_PAIRS 8		   /* 最大 PTY ペア数 */
#define PTY_MASTER_FD_BASE 200 /* PTY master fd の開始番号 */
#define PTY_SLAVE_FD_BASE 216  /* PTY slave fd の開始番号 */

void pty_reset(void);
int pty_open(int *master_fd, int *slave_fd);
int pty_is_master_fd(int fd);
int pty_is_slave_fd(int fd);
int pty_is_fd(int fd);
long pty_read(int fd, char *buf, unsigned int size);
long pty_write(int fd, const char *buf, unsigned int size);
pid_t pty_get_foreground_pgrp_for_slave_fd(int slave_fd);
int pty_set_foreground_pgrp_for_slave_fd(int slave_fd, pid_t pgrp);

#endif /* _PTY_H */
