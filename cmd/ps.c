#include <kfs/pid.h>
#include <kfs/stddef.h>

#define KFS_PS_TTY_LEN 16
#define KFS_PS_TIME_LEN 16
#define KFS_PS_STAT_LEN 8
#define KFS_PS_CMD_LEN 128

/* ps がユーザー空間で保持する表示用レコード */
struct kfs_ps_entry
{
	pid_t pid;
	pid_t ppid;
	char tty[KFS_PS_TTY_LEN];
	char time[KFS_PS_TIME_LEN];
	char stat[KFS_PS_STAT_LEN];
	char cmd[KFS_PS_CMD_LEN];
};