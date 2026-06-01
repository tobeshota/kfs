#include <kfs/errno.h>
#include <kfs/printk.h>
#include <kfs/ps.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

/** ps コマンド（ユーザランド）エントリ
 * @brief Phase1 の骨組み。`ps_snapshot` を呼んで1件だけ取得する。
 * @details 後続コミットで引数解析・複数件取得・整形表示を追加する予定。
 */
void cmd_ps(const char *args)
{
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
			printk("Usage: ps [-l]\n");
			return;
		}
		else
		{
			/* -l/--help 以外はヘルプ出力とする */
			printk("Usage: ps [-l]\n");
			return;
		}
	}
	/* 最大取得件数（スタック上確保） */
	enum
	{
		MAX_PS = 64,
	};
	struct kfs_ps_entry entries[MAX_PS];

	long n = ps_snapshot(entries, MAX_PS);
	if (n < 0)
	{
		printk("ps: snapshot failed\n");
		return;
	}

	if (long_mode)
	{
		printk("PID  PPID STAT TTY      TIME  CMD\n");
		for (long i = 0; i < n; i++)
		{
			struct kfs_ps_entry *e = &entries[i];
			/* PID PPID STAT TTY TIME CMD */
			printk("%5d %4d %4s %7s %5s %s\n", (int)e->pid, (int)e->ppid, e->stat, e->tty, e->time, e->cmd);
		}
	}
	else
	{
		/* ヘッダ（無指定モード） */
		printk("PID   TTY      TIME  CMD\n");

		for (long i = 0; i < n; i++)
		{
			struct kfs_ps_entry *e = &entries[i];
			/* フォーマット: PID  TTY  TIME  CMD */
			printk("%5d %7s %5s %s\n", (int)e->pid, e->tty, e->time, e->cmd);
		}
	}
}
