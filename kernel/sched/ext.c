#include <asm-i386/system.h>
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

/** sched_ext のスイッチモード
 * @brief sched_init() で SCHED_EXT_MODE_FULL に初期化され，
 *        sched_ext_register() で backend の flags をもとに設定される．
 */
static enum sched_ext_switch_mode sched_ext_mode = SCHED_EXT_MODE_FULL;

/** backend所有プロセス
 * @brief sched_ext_unregister()で0に初期化され，
 *        sched_ext_load()でロードしたプロセスのpidに設定される．
 */
static pid_t sched_ext_owner_pid;

/* fallback理由 */
enum sched_ext_fallback_reason
{
	SCHED_EXT_FALLBACK_NONE = 0,	   /* 正常なunload */
	SCHED_EXT_FALLBACK_DISPATCH_EMPTY, /* dispatchが空 */
	SCHED_EXT_FALLBACK_INVALID_TASK,   /* 無効なタスク */
};

static enum sched_ext_fallback_reason sched_ext_last_fallback;

/* sched_ext がサポートするポリシーか確認する */
static int sched_ext_policy_supported(unsigned int policy)
{
	return policy == SCHED_NORMAL || policy == SCHED_BATCH || policy == SCHED_IDLE || policy == SCHED_EXT;
}

/** sched_ext が管理するポリシーか確認する
 * @param policy スケジューリングポリシー
 * @param mode スイッチモード
 * @return 1=管理する, 0=管理しない
 * @example
 * mode が SCHED_EXT_MODE_PARTIAL の場合は SCHED_EXT のみ管理する
 * mode が SCHED_EXT_MODE_FULL の場合はすべての sched_ext_policy_supported() なポリシーを管理する
 */
static int sched_ext_policy_managed(unsigned int policy, enum sched_ext_switch_mode mode)
{
	/* sched_ext backend が無効ならば管理しない */
	if (!sched_ext_policy_supported(policy))
	{
		return 0;
	}

	/* mode が SCHED_EXT_MODE_PARTIAL の場合は
	 * SCHED_EXT のみ管理する */
	if (mode == SCHED_EXT_MODE_PARTIAL)
	{
		return policy == SCHED_EXT;
	}

	/* mode が SCHED_EXT_MODE_FULL の場合は
	 * すべての sched_ext_policy_supported() なポリシーを管理する */
	return 1;
}

/** fallback理由を表示用文字列へ変換する
 * @param reason fallback理由
 * @return reasonに対応する固定文字列
 */
static const char *sched_ext_fallback_reason_name(enum sched_ext_fallback_reason reason)
{
	switch (reason)
	{
	case SCHED_EXT_FALLBACK_DISPATCH_EMPTY:
		return "dispatch_empty";
	case SCHED_EXT_FALLBACK_INVALID_TASK:
		return "invalid_task";
	case SCHED_EXT_FALLBACK_NONE:
	default:
		return "none";
	}
}

/** fair fallback中のSCHED_EXT taskをbackendへ移送する
 * @param task 移送対象task
 * @param ctx 未使用
 * @return 常に0
 */
static int sched_ext_migrate_task_from_fair(struct task_struct *task, void *ctx)
{
	(void)ctx;

	/* mode で管理対象外の task や fair runqueue にいない task は何もしない */
	if (!sched_ext_policy_managed(task->policy, sched_ext_mode) || !fair_sched_class.task_queued(task))
	{
		return 0;
	}

	/* fair scheduler から backend へ移送する */
	fair_sched_class.dequeue_task(task);
	sched_ext_ops->enqueue_task(task);

	return 0;
}

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

	unsigned long flags;
	local_irq_save(flags);

	/* 既に backend が登録されている場合は解除する */
	sched_ext_unregister();

	int ret = ops->init();
	if (ret)
	{
		/* 新しい backend の初期化に失敗した場合は何もしない */
		local_irq_restore(flags);
		return ret;
	}

	sched_ext_ops = ops;
	sched_ext_mode = (ops->flags & SCX_OPS_SWITCH_PARTIAL) ? SCHED_EXT_MODE_PARTIAL : SCHED_EXT_MODE_FULL;
	task_for_each(sched_ext_migrate_task_from_fair, NULL);
	local_irq_restore(flags);
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

	/* mode で管理対象外の task や backend runqueue にいない task は何もしない */
	if (!sched_ext_policy_managed(task->policy, sched_ext_mode) || !sched_ext_ops->task_queued(task))
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

/** sched_ext backendを停止してtaskをfairへfallbackする
 * @param reason 停止理由。正常なunloadではSCHED_EXT_FALLBACK_NONE
 */
static void sched_ext_disable(enum sched_ext_fallback_reason reason)
{
	if (!sched_ext_ops)
	{
		sched_ext_last_fallback = reason;
		return;
	}

	unsigned long flags;
	local_irq_save(flags);

	task_for_each(sched_ext_migrate_task_to_fair, NULL);
	sched_ext_ops->exit();
	sched_ext_ops = NULL;
	sched_ext_owner_pid = 0;
	sched_ext_last_fallback = reason;
	local_irq_restore(flags);
}

/** sched_ext backendを正常に解除する
 * @note backend未登録の場合はfallback理由をnoneへ戻す
 */
void sched_ext_unregister(void)
{
	sched_ext_disable(SCHED_EXT_FALLBACK_NONE);
}

/** sched_ext backend が有効か確認する
 * @return 1=有効, 0=無効
 */
int sched_ext_enabled(void)
{
	return sched_ext_ops != NULL;
}

/* sched_ext のスイッチモードを取得する */
enum sched_ext_switch_mode sched_ext_switch_mode(void)
{
	return sched_ext_mode;
}

/* sched_ext のスイッチモード名を取得する */
const char *sched_ext_mode_name(void)
{
	if (sched_ext_mode == SCHED_EXT_MODE_PARTIAL)
	{
		return "partial";
	}
	return "full";
}

/** sched_ext backend を使用するか確認する
 * @param policy スケジューリングポリシー
 * @return 1=使用する, 0=使用しない
 */
int sched_ext_use_ext_class_for_policy(unsigned int policy)
{
	/* sched_ext backend が無効ならば使用しない */
	if (!sched_ext_enabled())
	{
		return 0;
	}

	return sched_ext_policy_managed(policy, sched_ext_mode);
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
	strlcpy(status->mode, status->enabled ? sched_ext_mode_name() : "none", sizeof(status->mode));
	strlcpy(status->fallback_reason, sched_ext_fallback_reason_name(sched_ext_last_fallback),
			sizeof(status->fallback_reason));
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
	sched_ext_mode = SCHED_EXT_MODE_FULL;
}

/** SCHED_EXT task を runqueue に追加する
 * @param task 追加する task
 * @details backend 未登録時は fair scheduler へ fallback する
 */
static void ext_enqueue_task(struct task_struct *task)
{
	if (!sched_ext_policy_supported(task->policy))
	{
		fair_sched_class.enqueue_task(task);
		return;
	}

	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		fair_sched_class.enqueue_task(task);
		return;
	}

	if (!sched_ext_policy_managed(task->policy, sched_ext_mode))
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
	if (!sched_ext_policy_supported(task->policy))
	{
		fair_sched_class.dequeue_task(task);
		return;
	}

	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		fair_sched_class.dequeue_task(task);
		return;
	}

	if (!sched_ext_policy_managed(task->policy, sched_ext_mode))
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
	if (!sched_ext_policy_supported(task->policy))
	{
		return fair_sched_class.task_queued(task);
	}

	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		return fair_sched_class.task_queued(task);
	}

	if (!sched_ext_policy_managed(task->policy, sched_ext_mode))
	{
		return fair_sched_class.task_queued(task);
	}

	return sched_ext_ops->task_queued(task);
}

/** backendにqueuedされたtaskがあるか確認する
 * @param task 確認対象task
 * @param ctx 未使用
 * @return backendにqueuedされたSCHED_EXT taskなら1，それ以外は0
 */
static int sched_ext_find_queued_task(struct task_struct *task, void *ctx)
{
	(void)ctx;

	if (sched_ext_policy_managed(task->policy, sched_ext_mode) && sched_ext_ops->task_queued(task))
	{
		return 1;
	}
	return 0;
}

/** backendが返したtaskがグローバルな task リストに存在するか確認する
 * @param task task list上のtask
 * @param ctx backendが返したtask pointer
 * @return pointerが一致すれば1，それ以外は0
 */
static int sched_ext_find_task(struct task_struct *task, void *ctx)
{
	return task == (struct task_struct *)ctx;
}

/** sched_ext backendから次のtaskを取得する
 * @return 次に実行するtask。backend未登録または空ならNULL
 * @note backend bookkeepingに矛盾があればbackendを停止してfairへfallbackする
 */
static struct task_struct *ext_pick_next_task(void)
{
	struct task_struct *task;

	if (!sched_ext_ops)
	{
		return NULL;
	}

	task = sched_ext_ops->pick_next_task();

	/* backend が返した task が NULL の場合は
	 * fair scheduler へ fallback する */
	if (!task)
	{
		if (task_for_each(sched_ext_find_queued_task, NULL) > 0)
		{
			sched_ext_disable(SCHED_EXT_FALLBACK_DISPATCH_EMPTY);
			return fair_sched_class.pick_next_task();
		}
		return NULL;
	}

	/* backend が返した task がグローバルな task リストに存在しない場合や
	 * 状態が不正な場合は
	 * fair scheduler へ fallback する */
	if (task_for_each(sched_ext_find_task, task) <= 0 || !sched_ext_policy_managed(task->policy, sched_ext_mode) ||
		task->__state != TASK_RUNNING || !sched_ext_ops->task_queued(task))
	{
		sched_ext_disable(SCHED_EXT_FALLBACK_INVALID_TASK);
		return fair_sched_class.pick_next_task();
	}

	return task;
}

/** SCHED_EXT task の tick 処理を行う
 * @param task tick 処理対象
 * @details backend 未登録時は fair scheduler へ fallback する
 */
static void ext_task_tick(struct task_struct *task)
{
	if (!sched_ext_policy_supported(task->policy))
	{
		fair_sched_class.task_tick(task);
		return;
	}

	/* backend 未登録の場合は fair scheduler へ fallback する */
	if (!sched_ext_ops)
	{
		fair_sched_class.task_tick(task);
		return;
	}

	if (!sched_ext_policy_managed(task->policy, sched_ext_mode))
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
