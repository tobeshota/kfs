#include <kfs/errno.h>
#include <kfs/exit.h>
#include <kfs/fair.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/sched_ext.h>
#include <kfs/stddef.h>
#include <kfs/string.h>

/** 現在有効な sched_ext backend
 * @brief sched_ext_register() で登録され，
 *        sched_ext_unregister() で解除される．
 */
static const struct sched_ext_ops *sched_ext_ops;

/** backend所有プロセス
 * @brief sched_ext_unregister()で0に初期化され，
 *        sched_ext_load()でロードしたプロセスのpidに設定される．
 */
static pid_t sched_ext_owner_pid;

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

/** SCHED_EXT taskをbackendからfairへ移送する
 * @param task 移送対象task
 * @param ctx 未使用
 * @return 常に0
 */
static int sched_ext_migrate_task_to_fair(struct task_struct *task, void *ctx)
{
	(void)ctx;

	/* SCHED_EXT task でない場合や backend にキューされていない場合は何もしない */
	if (task->policy != SCHED_EXT || !sched_ext_ops->task_queued(task))
	{
		return 0;
	}

	sched_ext_ops->dequeue_task(task);
	/** task が実行中の場合は fair scheduler へ移送する
	 * @note task が実行中でない場合は fair scheduler へ移送しない
	 *      （fair scheduler の runqueue に載せない）
	 *       これは、task が実行中でない場合は fair scheduler の runqueue に載せると、
	 *       次の tick で fair scheduler が task を実行してしまうためである
	 */
	if (task->__state == TASK_RUNNING)
	{
		fair_sched_class.enqueue_task(task);
	}
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

	task_for_each(sched_ext_migrate_task_to_fair, NULL);
	sched_ext_ops->exit();
	sched_ext_ops = NULL;
	sched_ext_owner_pid = 0;
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

/** sched_ext所有プロセスの終了を処理する
 * @param task 終了するtask
 */
static void sched_ext_owner_exit(struct task_struct *task)
{
	/* sched_ext が有効で、かつ呼び出し元プロセスのPIDがbackend所有プロセスである場合 */
	if (sched_ext_enabled() && task->pid == sched_ext_owner_pid)
	{
		sched_ext_unregister();
	}
}

/** 名前を指定してsched_ext backendをロードする
 * @param name backend名
 * @return 0=成功, 負数=エラー
 * @note sched_ext_owner_pid はロードしたプロセスの pid に設定される
 */
int sys_sched_ext_load(const char *name)
{
	/* 名前が無効な場合はエラーを返す */
	if (!name || strcmp(name, "pure_rr") != 0)
	{
		return -EINVAL;
	}

	/* sched_ext が既に有効な場合はエラーを返す */
	if (sched_ext_enabled())
	{
		return -EPERM;
	}

	int ret = sched_ext_register(&sched_ext_pure_rr_ops);
	if (ret)
	{
		return ret;
	}

	/* sched_ext_owner_pid を設定する */
	sched_ext_owner_pid = current->pid;

	return 0;
}

/** 呼び出しプロセスが所有するsched_ext backendを解除する
 * @return 0=成功, 負数=エラー
 */
int sys_sched_ext_unload(void)
{
	if (!sched_ext_enabled())
	{
		return 0;
	}

	/* 呼び出しプロセスが backend 所有プロセスでない場合はエラーを返す
	 * その理由は，backend の所有権を持つプロセスのみが backend を解除できるようにするため．
	 */
	if (sched_ext_owner_pid != current->pid)
	{
		return -EPERM;
	}

	sched_ext_unregister();
	return 0;
}

/** sched_extの現在状態を取得する
 * @param status 状態の格納先
 * @return 0=成功, 負数=エラー
 */
int sys_sched_ext_status(struct sched_ext_status *status)
{
	if (!status)
	{
		return -EINVAL;
	}

	status->enabled = sched_ext_enabled();
	status->owner_pid = sched_ext_owner_pid;
	strlcpy(status->name, sched_ext_name(), sizeof(status->name));
	return 0;
}

/** sched_ext class を初期化する
 * @note 起動時や単体テストの再初期化では backend 未登録状態に戻す
 */
static void ext_init(void)
{
	static int exit_hook_registered;

	/* sched_ext が有効でない場合は exit hook を登録する．
	 * exit_hook_registeredが必要な理由は，
	 * exit hook が複数回登録されるのを防ぐためである．
	 */
	if (!exit_hook_registered)
	{
		/* exit hook を登録する． */
		register_exit_hook(sched_ext_owner_exit);
		exit_hook_registered = 1;
	}

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
