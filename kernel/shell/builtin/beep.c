#include <kfs/exec.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/sched.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
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
static void beep_ring3_main(void)
{
	msleep(1000);
	exit(0);
}

void cmd_beep(const char *args)
{
	while (*args == ' ')
	{
		args++;
	}
	if (*args == '\0')
	{
		printk("Usage: beep <freq_hz>  (e.g. beep 440)\n");
		return;
	}
	int freq = atoi(args);
	if (freq <= 0)
	{
		do_psg_stop(0);
		printk("beep: stopped\n");
		return;
	}
	printk("beep: %d Hz\n", freq);
	do_psg_note(0, (uint32_t)freq, 0);
	do_fork((unsigned long)beep_ring3_main);
	do_wait(NULL, 0);
	do_psg_stop(0);
}
