#include <kfs/errno.h>
#include <kfs/printk.h>
#include <kfs/ps.h>
#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

enum
{
	MAX_PS = 64,
};

/* cmd_ps runs on a 4KB kernel stack during syscall/interrupt transitions. */
static struct kfs_ps_entry g_ps_entries[MAX_PS];
static char g_ps_line[256];

/** ps コマンド（ユーザランド）エントリ
 * @brief Phase1 の骨組み。`ps_snapshot` を呼んで1件だけ取得する。
 * @details 後続コミットで引数解析・複数件取得・整形表示を追加する予定。
 */
void cmd_ps(void *arg)
{
	const char *args = arg;
	/* 引数先頭の空白をスキップ */
	while (args && *args == ' ')
	{
		args++;
	}
	int long_mode = 0;
	if (args && *args != '\0')
	{
		if (strcmp(args, "-l") == 0)
		{
			long_mode = 1;
		}
		else if (strcmp(args, "--help") == 0)
		{
			printf("Usage: ps [-l]\n");
			return;
		}
		else
		{
			printf("Usage: ps [-l]\n");
			return;
		}
	}
	long n = ps_snapshot(g_ps_entries, MAX_PS);
	if (n < 0)
	{
		printf("ps: snapshot failed\n");
		return;
	}

	if (long_mode)
	{
		printf("PID\tPPID\tSTAT\tTTY\tTIME\tCMD\n");
		for (long i = 0; i < n; i++)
		{
			struct kfs_ps_entry *e = &g_ps_entries[i];
			snprintf(g_ps_line, sizeof(g_ps_line), "%5d\t%4d\t%4s\t%7s\t%5s\t%s\n", (int)e->pid, (int)e->ppid, e->stat,
					 e->tty, e->time, e->cmd);
			printf("%s", g_ps_line);
		}
	}
	else
	{
		printf("PID\tTTY\tTIME\tCMD\n");

		for (long i = 0; i < n; i++)
		{
			struct kfs_ps_entry *e = &g_ps_entries[i];
			printf("%5d\t%7s\t%5s\t%s\n", (int)e->pid, e->tty, e->time, e->cmd);
		}
	}
}
