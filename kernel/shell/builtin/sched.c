#include <kfs/exec.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/shell.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

static void putstr(void *s)
{
	write(1, s, strlen(s));
}

/** sched コマンド: ユーザ空間におけるプロセスのライフサイクルをテストする
 * @brief ring-3において，プロセスがfork()で誕生し，exec_fn()で生まれ変わり，
 *        exit()で終了し，親のwait()によって揮発するまでの全過程が意図通りであることを確かめる．
 *        期待する出力: "-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_\n"
 */
void sched_ring3_main(void)
{
	setpgid(0, 0);
	for (int i = 0; i < 20; i++)
	{
		/* プロセスを誕生させる */
		pid_t pid = fork();
		if (pid < 0)
		{
			write(1, "Failed to fork process\n", 23);
			exit(1);
		}
		else if (pid == 0)
		{
			/* 子プロセスは"-"を出力する */
			exec_fn(putstr, (void *)"-");
		}
		else
		{
			/* 親プロセスは終了した子プロセスを回収後，"_"を出力する */
			wait(NULL);
			write(1, "_", 1);
		}
	}
	write(1, "\n", 1);

	/* 親プロセスが終了する */
	exit(0);
}

void cmd_sched(const char *args, int foreground)
{
	(void)args;
	shell_launch_ring3_job("sched_ring3_main", sched_ring3_main, foreground);
}
