#include <kfs/sched_ext.h>
#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/unistd.h>

/** sched_extの現在状態を表示する
 * @param arg コマンド引数
 */
void cmd_sched_ext_status(void *arg)
{
	struct sched_ext_status status;
	const char *args = (const char *)arg;

	while (*args == ' ')
	{
		args++;
	}

	/* 引数が無効な場合は使用方法を表示する */
	if (*args != '\0')
	{
		printf("Usage: sched_ext_status\n");
		return;
	}

	if (sched_ext_status(&status) < 0)
	{
		printf("sched_ext_status: failed to read status\n");
		return;
	}

	printf("enabled: %s\n", status.enabled ? "yes" : "no");
	printf("backend: %s\n", status.name);
	printf("owner: %d\n", status.owner_pid);
}
