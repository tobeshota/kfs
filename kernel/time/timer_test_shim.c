/*
 * タイマーキューのテスト用ヘルパー
 */
#include <kfs/list.h>
#include <kfs/timer.h>

extern struct list_head timer_queue; /* kernel/time/timer.c で定義 */

/* テスト用にタイマーキューと jiffies をリセットする */
void timer_queue_reset(void)
{
	INIT_LIST_HEAD(&timer_queue);
	jiffies = 0;
}
