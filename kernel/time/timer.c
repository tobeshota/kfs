#include <kfs/list.h>
#include <kfs/sched.h>
#include <kfs/timer.h>

/** グローバルタイマーキュー
 * @note expiresで昇順ソート済み
 */
LIST_HEAD(timer_queue);

/** タイマーを初期化する
 * @param timer  初期化するタイマー
 * @param fn     満了時コールバック
 */
void timer_setup(struct timer_list *timer, void (*fn)(struct timer_list *))
{
	INIT_LIST_HEAD(&timer->entry);
	timer->function = fn;
	timer->data = (void *)0;
	timer->expires = 0;
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
		{
			break;
		}
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

/** タイマー満了時に呼ばれ、スリープ中のプロセスを起こすコールバック
 * @param t 満了したタイマー（data フィールドに task_struct * が入っている）
 */
static void process_timeout(struct timer_list *t)
{
	struct task_struct *task = (struct task_struct *)t->data;

	wake_up_process(task);
}

/** 指定 tick 後に現在プロセスを起こすスリープ
 * @param timeout スリープ tick 数（HZ=1000 なら ms と等しい）
 * @return 残り tick 数（0 なら正確に満了、負なら超過）
 */
long schedule_timeout(long timeout)
{
	struct timer_list timer;
	long expire;

	expire = (long)jiffies + timeout;
	timer_setup(&timer, process_timeout);
	timer.expires = (uint32_t)expire;
	timer.data = (void *)current; /* process_timeout() に渡すプロセス */
	add_timer(&timer);

	current->__state = TASK_INTERRUPTIBLE;
	schedule();

	del_timer(&timer);           /* 早起きした場合のクリーンアップ */
	current->__state = TASK_RUNNING;
	return expire - (long)jiffies; /* 残り tick（負なら超過） */
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
		{
			break;
		}

		list_del_init(&t->entry); /* キューから外してから */
		t->function(t);			  /* コールバック実行 */
	}
}
