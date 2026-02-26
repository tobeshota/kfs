#include <kfs/list.h>
#include <kfs/timer.h>

/** グローバルタイマーキュー
 * @note expiresで昇順ソート済み
 */
static LIST_HEAD(timer_queue);

/** タイマーを初期化する
 * @param timer  初期化するタイマー
 * @param fn     満了時コールバック
 */
void timer_setup(struct timer_list *timer, void (*fn)(struct timer_list *))
{
	INIT_LIST_HEAD(&timer->entry);
	timer->function = fn;
	timer->data     = (void *)0;
	timer->expires  = 0;
}

/** タイマーをキューに expires 昇順で挿入する
 * @brief list_head timer_queue に timer を expires 昇順で挿入する．
 * @param timer 挿入するタイマー
 * @note CLI/STI で割り込みコンテキストとの競合を防ぐ．
 */
void add_timer(struct timer_list *timer)
{
	struct list_head *pos;
	struct list_head *tmp;

	__asm__ volatile("cli"); /* 割り込み禁止 */
	list_for_each_safe(pos, tmp, &timer_queue)
	{
		struct timer_list *t = list_entry(pos, struct timer_list, entry);
		/* timer->expires が t->expires より前（小さい）なら t の直前に挿入 */
		if (time_before(timer->expires, t->expires))
			break;
	}
	/* pos の直前に挿入（昇順を維持）。
	 * pos == &timer_queue のときはリスト末尾への追加になる。 */
	list_add_tail(&timer->entry, pos);
	__asm__ volatile("sti"); /* 割り込み許可 */
}

/** タイマーをキューから削除する（未満了でもよい）
 * @note CLI/STI で割り込みコンテキストとの競合を防ぐ。
 */
void del_timer(struct timer_list *timer)
{
	__asm__ volatile("cli");
	list_del_init(&timer->entry);
	__asm__ volatile("sti");
}

/** 満了済みタイマーを実行する
 * @brief timer_interrupt() から毎 tick 呼ばれる。
 *        キューは昇順なので expires > jiffies の時点で break できる。
 */
void run_local_timers(void)
{
	struct list_head *pos;
	struct list_head *tmp;

	list_for_each_safe(pos, tmp, &timer_queue)
	{
		struct timer_list *t = list_entry(pos, struct timer_list, entry);

		/* 昇順なので最初に expires > jiffies が来たら以降は全部未満了 */
		if (time_after(t->expires, jiffies))
			break;

		list_del_init(&t->entry); /* キューから外してから */
		t->function(t);           /* コールバック実行 */
	}
}
