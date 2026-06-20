#ifndef _KFS_SCHED_EXT_H
#define _KFS_SCHED_EXT_H

#include <kfs/pid.h>

#define SCHED_EXT_NAME_LEN 16
#define SCHED_EXT_FALLBACK_REASON_LEN 32

struct sched_class;
struct task_struct;

/** sched_ext backend operations
 * @brief SCHED_EXT task の実際のスケジューリングを担う backend の操作集合
 * @note ops は operations の略称
 */
struct sched_ext_ops
{
	const char *name;								/* backend 名 */
	int (*init)(void);								/* backend を有効化する */
	void (*exit)(void);								/* backend を無効化する */
	void (*enqueue_task)(struct task_struct *task); /* task を backend runqueue に追加する */
	void (*dequeue_task)(struct task_struct *task); /* task を backend runqueue から削除する */
	int (*task_queued)(struct task_struct *task);	/* task が backend runqueue にいるか確認する */
	struct task_struct *(*pick_next_task)(void);	/* 次に実行する task を選択する */
	void (*task_tick)(struct task_struct *task);	/* task の tick 処理を行う */
};

/* sched_extの現在状態 */
struct sched_ext_status
{
	int enabled;										 /* 1=有効, 0=無効 */
	pid_t owner_pid;									 /* backend所有プロセス */
	char name[SCHED_EXT_NAME_LEN];						 /* backend名 */
	char fallback_reason[SCHED_EXT_FALLBACK_REASON_LEN]; /* 最後に異常fallbackした理由 */
};

extern const struct sched_class sched_ext_class;

int sched_ext_register(const struct sched_ext_ops *ops);
void sched_ext_unregister(void);
int sched_ext_enabled(void);
const char *sched_ext_name(void);
int sys_sched_ext_load(const char *name);
int sys_sched_ext_unload(void);
int sys_sched_ext_status(struct sched_ext_status *status);

#endif /* _KFS_SCHED_EXT_H */
