#include <kfs/exec.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

/** sleep コマンド用 ring-3 エントリポイント
 * @note msleep() は int $0x80 経由の ring-3 ラッパーなので，
 *       ring-3 コンテキストから呼ぶ必要がある
 */
static unsigned int g_sleep_ms; /* cmd_sleep → sleep_ring3_main へのパラメータ渡し用 */

static void sleep_ring3_main(void)
{
	msleep(g_sleep_ms);
	exit(0);
}

/** sleep コマンド: 指定秒数だけ CPU を手放して待機する
 * 用法: sleep <秒>
 * @note ring-3 の msleep() 経路（int $0x80 → sys_msleep → schedule_timeout）が
 *       正しく機能することを確かめるため，子プロセスを fork して msleep() を呼ばせる．
 *       子が TASK_INTERRUPTIBLE でスリープ中はランキューが空になり
 *       do_wait() 内の schedule() が 0 を返して -EAGAIN になるため，
 *       親は hlt でタイマー割り込みを待ちながらリトライする．
 */
void cmd_sleep(const char *args)
{
	while (*args == ' ')
	{
		args++;
	}
	if (*args == '\0')
	{
		printk("Usage: sleep <seconds>\n");
		return;
	}
	int secs = atoi(args);
	if (secs <= 0)
	{
		printk("sleep: invalid duration\n");
		return;
	}
	g_sleep_ms = (unsigned int)secs * 1000;
	/* ring-3 へ降りて msleep() を呼ばせ，終了を待つ */
	do_fork((unsigned long)sleep_ring3_main);
	do_wait(NULL, 0);
}
