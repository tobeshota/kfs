#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>

/** 空白だけで構成される引数か確認する
 * @param args コマンド引数
 * @return 1=空または空白のみ, 0=余分な引数あり
 */
static int spin_args_empty(const char *args)
{
	while (*args == ' ')
	{
		args++;
	}
	return *args == '\0';
}

/** CPUを使い続ける
 * @brief スケジューラの挙動確認用に，出力や sleep を挟まず計算だけを続ける．
 */
static void spin_main(void)
{
	/* volatile をつけることで，
	 * コンパイラは counter++; (誰も参照せず，よって観測可能な動作に影響しない計算)を
	 * 最適化のために削除しないようにする */
	volatile unsigned long counter = 0;
	while (1)
	{
		counter++;
	}
}

/** spin コマンド
 * @param arg コマンド引数。現時点では引数なしのみ受け付ける
 * @note spin は必ず & 付きで起動されることを想定している．
 *       フォアグラウンドで起動するとCPUをあまりにも使い続けるためシェルに戻れない．
 */
void cmd_spin(void *arg)
{
	const char *args = (const char *)arg;

	/* 引数が空でない場合は使用方法を表示して終了 */
	if (!spin_args_empty(args))
	{
		printf("Usage: spin\n");
		return;
	}

	spin_main();
}
