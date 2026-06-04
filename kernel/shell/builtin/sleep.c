#include <kfs/exec.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/shell.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

/** sleep コマンド用 ring-3 エントリポイント
 * @note msleep() は int $0x80 経由の ring-3 ラッパーなので，
 *       ring-3 コンテキストから呼ぶ必要がある
 */
static unsigned int g_sleep_ms; /* cmd_sleep → sleep_ring3_main へのパラメータ渡し用 */

void sleep_set_ms(unsigned int ms)
{
	g_sleep_ms = ms;
}

void sleep_ring3_main(void)
{
	setpgid(0, 0);
	msleep(g_sleep_ms);
	exit(0);
}

static int parse_positive_int(const char *args, int *out)
{
	while (*args == ' ')
	{
		args++;
	}
	if (*args == '\0')
	{
		return 0;
	}
	int value = atoi(args);
	if (value <= 0)
	{
		return 0;
	}
	*out = value;
	return 1;
}

void cmd_sleep(const char *args, int foreground)
{
	int secs;

	if (!parse_positive_int(args, &secs))
	{
		printk("Usage: sleep <seconds>\n");
		return;
	}
	sleep_set_ms((unsigned int)secs * 1000);
	shell_launch_ring3_job("sleep_ring3_main", sleep_ring3_main, foreground);
}
