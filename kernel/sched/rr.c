/**
 * @file rr.c
 * @brief 純粋ラウンドロビン（SCHED_PURE_RR）スケジューラ（Phase 7）
 *
 * 固定タイムスライスによる循環型スケジューリングを実装する．
 * - 実行可能なタスクを双方向リスト（rr_runqueue）で管理する
 * - タイムスライスが切れたタスクはキューの末尾に再挿入される
 * - コンテキストスイッチの物理的な切り替えは Phase 8 で実装する
 *   （本 Phase では current ポインタの更新のみ）
 */

#include <kfs/list.h>
#include <kfs/rr.h>
#include <kfs/sched.h>

/* RR ランキュー（実行可能タスクの侵入型双方向循環リスト） */
static struct list_head rr_runqueue;

/** RR スケジューラを初期化する
 * @brief ランキューのリストヘッドを初期化する．
 *        kernel/sched/core.c の sched_init() から一度だけ呼ばれる．
 */
void rr_init(void)
{
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
