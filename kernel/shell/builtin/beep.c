#include <kfs/exec.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/sched.h>
#include <kfs/shell.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

/** beep コマンド: 指定周波数の矩形波を 1 秒間鳴らす
 * @param args コマンド名以降の文字列（周波数文字列または空文字列）
 *
 * @note
 *   コマンド       周波数     音名
 *   beep          -         使い方を表示
 *   beep 0        -         停止
 *   beep 262      262 Hz    C4（ド）
 *   beep 330      330 Hz    E4（ミ）
 *   beep 392      392 Hz    G4（ソ）
 *   beep 440      440 Hz    A4（ラ）← 国際標準チューニング基準音
 *   beep 494      494 Hz    B4（シ）
 *   beep 523      523 Hz    C5（高いド）
 */
static char g_beep_args[32];

void beep_set_args(const char *args)
{
	size_t i = 0;
	if (!args)
	{
		g_beep_args[0] = '\0';
		return;
	}
	/* コピー（先頭の空白は許容し、builtin 内で解析する） */
	while (i + 1 < sizeof(g_beep_args) && args[i] != '\0')
	{
		g_beep_args[i] = args[i];
		i++;
	}
	g_beep_args[i] = '\0';
}

void beep_ring3_main(void)
{
	setpgid(0, 0);

	/* args を解析する（builtin 内で処理する） */
	const char *s = g_beep_args;
	while (*s == ' ')
	{
		s++;
	}

	if (*s == '\0')
	{
		write(1, "Usage: beep <freq_hz>  (e.g. beep 440)\n", 40);
		exit(0);
	}

	int freq = atoi(s);
	if (freq <= 0)
	{
		/* 0 や負の値は停止扱い */
		psg_stop(0);
		exit(0);
	}

	write(1, "beep: ", 6);
	write(1, s, strlen(s));
	write(1, " Hz\n", 4);
	psg_note(0, (unsigned int)freq, 0);
	msleep(1000);
	psg_stop(0);
	exit(0);
}

void cmd_beep(const char *args, int foreground)
{
	beep_set_args(args);
	shell_launch_ring3_job("beep_ring3_main", beep_ring3_main, foreground);
}
