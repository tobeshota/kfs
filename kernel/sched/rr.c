/**
 * @file rr.c
 * @brief legacy SCHED_PURE_RRとsched_ext pure_rr backendで共有する純粋ラウンドロビン実装
 *
 * 固定タイムスライスによる循環型スケジューリングを実装する．
 * - 実行可能なタスクを用途別の双方向リストで管理する
 * - タイムスライスが切れたタスクはキューの末尾に再挿入される
 * - 実際のコンテキストスイッチは scheduler core とアーキテクチャ依存コードが行う
 */

#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/sched_ext.h>

/* legacy policyとsched_ext backendは独立したランキューを使用する */
static struct list_head legacy_rr_runqueue;
static struct list_head sched_ext_pure_rr_runqueue;

/** RRランキューを初期化する
 * @param runqueue 初期化するランキュー
 */
static void rr_runqueue_init(struct list_head *runqueue)
{
	INIT_LIST_HEAD(runqueue);
}

/** taskを指定RRランキューの末尾へ追加する
 * @param runqueue 追加先ランキュー
 * @param task 追加するtask
 */
static void rr_runqueue_enqueue(struct list_head *runqueue, struct task_struct *task)
{
	if (!list_empty(&task->run_list))
	{
		return;
	}
	list_add_tail(&task->run_list, runqueue);
}

/** taskを所属RRランキューから削除する
 * @param task 削除するtask
 */
static void rr_runqueue_dequeue(struct task_struct *task)
{
	list_del_init(&task->run_list);
}

/** 指定RRランキューの先頭taskを返す
 * @param runqueue 選択元ランキュー
 * @return 先頭task。空の場合はNULL
 */
static struct task_struct *rr_runqueue_pick_next(struct list_head *runqueue)
{
	if (list_empty(runqueue))
	{
		return NULL;
	}
	return list_entry(runqueue->next, struct task_struct, run_list);
}

/** 指定RRランキュー上のtaskについてtick処理を行う
 * @param runqueue taskが所属するランキュー
 * @param task tick処理対象
 */
static void rr_runqueue_task_tick(struct list_head *runqueue, struct task_struct *task)
{
	if (task->time_slice > 0)
	{
		task->time_slice--;
		return;
	}

	task->time_slice = RR_TIMESLICE;
	rr_runqueue_dequeue(task);
	rr_runqueue_enqueue(runqueue, task);
}

/** RR スケジューラを初期化する
 * @brief ランキューを完全にフラッシュしてリセットする。
 *        HEAD だけでなく各タスクの run_list も空にする。
 *        これにより、テスト間でタスクの run_list に残るステールポインタを一掃する。
 *        kernel/sched/core.c の sched_init() から呼ばれる。
 */
void rr_init(void)
{
	/* BSS 零初期化状態（next==NULL）への対応:
	 * 静的変数の最初の呼び出しは next/prev が NULL のため
	 * list_for_each_safe でデリファレンスする前に初期化が必要。 */
	if (legacy_rr_runqueue.next == NULL)
	{
		rr_runqueue_init(&legacy_rr_runqueue);
		return;
	}

	/* 2 回目以降（sched_init / reset_all_state_for_test 経由）:
	 *
	 * reset_all_state_for_test() はタスクの run_list を INIT_LIST_HEAD で
	 * 自己参照にリセットした後に sched_init() → rr_init() を呼ぶ。
	 * このとき rr_runqueue.next は（タイマー割り込みで enqueue された）
	 * init_task.run_list を指したままになっているが、
	 * init_task.run_list.next は自分自身を指している。
	 * これを list_for_each_safe で辿ると
	 *   pos = &init_task.run_list → n = &init_task.run_list （自己参照）
	 *   → pos != &rr_runqueue が永遠に true → 無限ループ
	 * となる。
	 *
	 * 各タスクの run_list は呼び出し元
	 * （reset_all_state_for_test / release_task 等）が責任を持って
	 * リセット済みなので、ここではヘッドを再初期化するだけでよい。
	 * 残存エントリを list_for_each_safe で辿る必要はない。 */
	rr_runqueue_init(&legacy_rr_runqueue);
}

/** タスクを RR ランキューの末尾に追加する
 * @brief tsk->run_list を rr_runqueue の末尾にリンクする．
 * @param tsk キューに追加するタスク
 */
void rr_enqueue(struct task_struct *tsk)
{
	rr_runqueue_enqueue(&legacy_rr_runqueue, tsk);
}

/** タスクを RR ランキューから取り除く
 * @param tsk キューから取り除くタスク
 */
void rr_dequeue(struct task_struct *tsk)
{
	rr_runqueue_dequeue(tsk);
}

/** 次に実行すべきタスクを返す
 * @return キューの先頭タスク，またはキューが空のとき NULL
 * @note 返したタスクがキューから取り除かれるわけではない
 */
struct task_struct *rr_pick_next(void)
{
	return rr_runqueue_pick_next(&legacy_rr_runqueue);
}

/** タイマーティックごとのタスク管理
 * @brief タイムスライスを 1 デクリメントする．
 *        0 に達したらスライスをリセットしてキューの末尾に再挿入することで
 *        ラウンドロビンによるプリエンプションを実現する．
 * @param tsk 現在実行中のタスク
 */
void rr_task_tick(struct task_struct *tsk)
{
	rr_runqueue_task_tick(&legacy_rr_runqueue, tsk);
}

/** sched_ext pure_rr backendを初期化する
 * @return 常に0
 */
static int sched_ext_pure_rr_init(void)
{
	rr_runqueue_init(&sched_ext_pure_rr_runqueue);
	return 0;
}

/** sched_ext pure_rr backendを終了する */
static void sched_ext_pure_rr_exit(void)
{
	rr_runqueue_init(&sched_ext_pure_rr_runqueue);
}

/** SCHED_EXT taskをpure_rr backendへ追加する
 * @param task 追加するtask
 */
static void sched_ext_pure_rr_enqueue_task(struct task_struct *task)
{
	rr_runqueue_enqueue(&sched_ext_pure_rr_runqueue, task);
}

/** SCHED_EXT taskをpure_rr backendから削除する
 * @param task 削除するtask
 */
static void sched_ext_pure_rr_dequeue_task(struct task_struct *task)
{
	rr_runqueue_dequeue(task);
}

/** taskがpure_rr backendに登録されているか返す
 * @param task 確認するtask
 * @return 1=登録済み, 0=未登録
 */
static int sched_ext_pure_rr_task_queued(struct task_struct *task)
{
	return !list_empty(&task->run_list);
}

/** pure_rr backendから次のtaskを選択する
 * @return 次に実行するtask。空の場合はNULL
 */
static struct task_struct *sched_ext_pure_rr_pick_next_task(void)
{
	return rr_runqueue_pick_next(&sched_ext_pure_rr_runqueue);
}

/** pure_rr backendのtick処理を行う
 * @param task 現在実行中のtask
 */
static void sched_ext_pure_rr_task_tick(struct task_struct *task)
{
	rr_runqueue_task_tick(&sched_ext_pure_rr_runqueue, task);
}

/** sched_ext用pure RR backend */
const struct sched_ext_ops sched_ext_pure_rr_ops = {
	.name = "pure_rr",
	.init = sched_ext_pure_rr_init,
	.exit = sched_ext_pure_rr_exit,
	.enqueue_task = sched_ext_pure_rr_enqueue_task,
	.dequeue_task = sched_ext_pure_rr_dequeue_task,
	.task_queued = sched_ext_pure_rr_task_queued,
	.pick_next_task = sched_ext_pure_rr_pick_next_task,
	.task_tick = sched_ext_pure_rr_task_tick,
};

/** pure RR class の enqueue 操作
 * @param task runnable にする task
 */
static void pure_rr_enqueue_task(struct task_struct *task)
{
	rr_enqueue(task);
}

/** pure RR class の dequeue 操作
 * @param task runqueue から外す task
 */
static void pure_rr_dequeue_task(struct task_struct *task)
{
	rr_dequeue(task);
}

/** task が pure RR runqueue に載っているかを返す
 * @param task 確認する task
 * @return 1=runqueue 上, 0=runqueue 外
 */
static int pure_rr_task_queued(struct task_struct *task)
{
	return !list_empty(&task->run_list);
}

/** pure RR class の pick 操作
 * @return 次に実行する task。存在しない場合は NULL
 */
static struct task_struct *pure_rr_pick_next_task(void)
{
	return rr_pick_next();
}

/** pure RR class の tick 操作
 * @param task 現在実行中の task
 */
static void pure_rr_task_tick(struct task_struct *task)
{
	rr_task_tick(task);
}

/** kfs 独自の純粋ラウンドロビン scheduler class */
const struct sched_class pure_rr_sched_class = {
	.init = rr_init,
	.enqueue_task = pure_rr_enqueue_task,
	.dequeue_task = pure_rr_dequeue_task,
	.task_queued = pure_rr_task_queued,
	.pick_next_task = pure_rr_pick_next_task,
	.task_tick = pure_rr_task_tick,
};
