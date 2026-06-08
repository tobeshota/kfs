#include <kfs/exec.h>
#include <kfs/sched.h>
#include <kfs/shell.h>
#include <kfs/stdint.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

static unsigned int g_sleep_ms; /* cmd_sleep → sleep_ring3_main へのパラメータ渡し用 */

void sleep_set_ms(unsigned int ms)
{
	g_sleep_ms = ms;
}

void sleep_ring3_main(void)
{
	unsigned int remain;
	int ret;

	setpgid(0, 0);
	remain = g_sleep_ms;
	while (remain > 0)
	{
		ret = msleep(remain);
		if (ret <= 0)
		{
			break;
		}
		remain = (unsigned int)ret;
	}
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

void cmd_sleep(void *arg)
{
	const char *args = arg;
	int secs;

	if (!parse_positive_int(args, &secs))
	{
		printf("Usage: sleep <seconds>\n");
		return;
	}
	sleep_set_ms((unsigned int)secs * 1000);
	sleep_ring3_main();
}
