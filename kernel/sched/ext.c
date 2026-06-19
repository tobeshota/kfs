#include <kfs/errno.h>
#include <kfs/fair.h>
#include <kfs/sched.h>
#include <kfs/sched_ext.h>
#include <kfs/stddef.h>

/* 現在有効な sched_ext backend */
static const struct sched_ext_ops *sched_ext_ops;

/** sched_ext backend ops が最低限の操作を持つか確認する
 * @param ops 確認する backend ops
 * @return 1=有効, 0=無効
 */
static int sched_ext_ops_valid(const struct sched_ext_ops *ops)
{
	/* sched_ext_opsの全メンバがNULLでないことを確認する */
	return ops && ops->name && ops->init && ops->exit && ops->enqueue_task && ops->dequeue_task && ops->task_queued &&
		   ops->pick_next_task && ops->task_tick;
}

/** sched_ext backend を登録して有効化する
 * @param ops 登録する backend ops
 * @return 0=成功, 負数=エラー
 */
int sched_ext_register(const struct sched_ext_ops *ops)
{
	if (!sched_ext_ops_valid(ops))
	{
		return -EINVAL;
	}

	/* 既に backend が登録されている場合は解除する */
	sched_ext_unregister();

	int ret = ops->init();
	if (ret)
	{
		/* 新しい backend の初期化に失敗した場合は何もしない */
		return ret;
	}

	sched_ext_ops = ops;
	return 0;
}

/** sched_ext backend を解除する
 * @note backend 未登録の場合は何もしない
 */
void sched_ext_unregister(void)
{
	/* backend 未登録の場合は何もしない */
	if (!sched_ext_ops)
	{
		return;
	}

	sched_ext_ops->exit();
	sched_ext_ops = NULL;
}

/** sched_ext backend が有効か確認する
 * @return 1=有効, 0=無効
 */
int sched_ext_enabled(void)
{
	return sched_ext_ops != NULL;
}

/** 現在有効な sched_ext backend 名を返す
 * @return backend 名。未登録なら "none"
 */
const char *sched_ext_name(void)
{
	if (!sched_ext_ops)
	{
		return "none";
	}
	return sched_ext_ops->name;
}

/** sched_ext class を初期化する
 * @note 起動時や単体テストの再初期化では backend 未登録状態に戻す
 */
static void ext_init(void)
{
	sched_ext_unregister();
}

/** SCHED_EXT task を runqueue に追加する
 * @param task 追加する task
 * @details backend 未登録時は fair scheduler へ fallback する
 */
static void ext_enqueue_task(struct task_struct *task)
{
	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		fair_sched_class.enqueue_task(task);
		return;
	}

	sched_ext_ops->enqueue_task(task);
}

/** SCHED_EXT task を runqueue から削除する
 * @param task 削除する task
 * @details backend 未登録時は fair scheduler へ fallback する
 */
static void ext_dequeue_task(struct task_struct *task)
{
	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		fair_sched_class.dequeue_task(task);
		return;
	}

	sched_ext_ops->dequeue_task(task);
}

/** SCHED_EXT task が runqueue に載っているか確認する
 * @param task 確認する task
 * @return 1=runqueue 上, 0=runqueue 外
 * @details backend 未登録時は fair scheduler の状態を返す
 */
static int ext_task_queued(struct task_struct *task)
{
	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		return fair_sched_class.task_queued(task);
	}

	return sched_ext_ops->task_queued(task);
}

/** sched_ext backend から次の task を取得する
 * @return 次に実行する task。backend 未登録または空なら NULL
 */
static struct task_struct *ext_pick_next_task(void)
{
	/* backend 未登録の場合は NULL を返す */
	if (!sched_ext_ops)
	{
		return NULL;
	}

	return sched_ext_ops->pick_next_task();
}

/** SCHED_EXT task の tick 処理を行う
 * @param task tick 処理対象
 * @details backend 未登録時は fair scheduler へ fallback する
 */
static void ext_task_tick(struct task_struct *task)
{
	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		fair_sched_class.task_tick(task);
		return;
	}

	sched_ext_ops->task_tick(task);
}

/** 拡張 scheduler class
 * @brief SCHED_EXT policy を backend ops へ委譲する scheduler class
 */
const struct sched_class sched_ext_class = {
	.init = ext_init,
	.enqueue_task = ext_enqueue_task,
	.dequeue_task = ext_dequeue_task,
	.task_queued = ext_task_queued,
	.pick_next_task = ext_pick_next_task,
	.task_tick = ext_task_tick,
};
