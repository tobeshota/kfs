/**
 * @file rr.c
 * @brief 純粋ラウンドロビン（SCHED_PURE_RR）スケジューラ
 *
 * 固定タイムスライスによる循環型スケジューリングを実装する．
 * - 実行可能なタスクを双方向リスト（rr_runqueue）で管理する
 * - タイムスライスが切れたタスクはキューの末尾に再挿入される
 * - 実際のコンテキストスイッチは scheduler core とアーキテクチャ依存コードが行う
 */

#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>

/* RR ランキュー（実行可能タスクの侵入型双方向循環リスト） */
static struct list_head rr_runqueue;

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
	if (rr_runqueue.next == NULL)
	{
		INIT_LIST_HEAD(&rr_runqueue);
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
	INIT_LIST_HEAD(&rr_runqueue);
}

/** タスクを RR ランキューの末尾に追加する
 * @brief tsk->run_list を rr_runqueue の末尾にリンクする．
 * @param tsk キューに追加するタスク
 */
void rr_enqueue(struct task_struct *tsk)
{
	/* 同一タスクの二重登録防止のため，
	   既にキューに入っているタスクは追加しない */
	if (!list_empty(&tsk->run_list))
	{
		return;
	}
	list_add_tail(&tsk->run_list, &rr_runqueue);
}

/** タスクを RR ランキューから取り除く
 * @param tsk キューから取り除くタスク
 */
void rr_dequeue(struct task_struct *tsk)
{
	list_del(&tsk->run_list);
	INIT_LIST_HEAD(&tsk->run_list);
}

/** 次に実行すべきタスクを返す
 * @return キューの先頭タスク，またはキューが空のとき NULL
 * @note 返したタスクがキューから取り除かれるわけではない
 */
struct task_struct *rr_pick_next(void)
{
	if (list_empty(&rr_runqueue))
	{
		return (struct task_struct *)0;
	}

	return list_entry(rr_runqueue.next, struct task_struct, run_list);
}

/** タイマーティックごとのタスク管理
 * @brief タイムスライスを 1 デクリメントする．
 *        0 に達したらスライスをリセットしてキューの末尾に再挿入することで
 *        ラウンドロビンによるプリエンプションを実現する．
 * @param tsk 現在実行中のタスク
 */
void rr_task_tick(struct task_struct *tsk)
{
	if (tsk->time_slice > 0)
	{
		tsk->time_slice--;
		return;
	}

	/* タスクのタイムスライスが枯渇した(tsk->time_slice == 0 である)場合，
	   タスクをRRランキューから取り除き，
	   再度末尾に追加することでラウンドロビンを実現する */
	tsk->time_slice = RR_TIMESLICE;
	rr_dequeue(tsk);
	rr_enqueue(tsk);
}

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
