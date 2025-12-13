#include <kfs/errno.h>
#include <kfs/mm.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/string.h>

/* 初期化マクロ（Phase 1では何もしない） */
#define __init

/** カーネルスタックサイズ（4KB = 1ページ） */
#define THREAD_SIZE 4096

/** task_struct用スラブキャッシュ
 * @note 頻繁に割り当て/解放されるため、スラブアロケータで高速化
 */
static struct kmem_cache *task_struct_cachep = NULL;

/** task_structを複製
 * @param orig コピー元のtask_struct
 * @return 新しいtask_struct（失敗時NULL）
 */
static struct task_struct *dup_task_struct(struct task_struct *orig)
{
	struct task_struct *tsk;
	void *stack;

	/* 新しいtask_structを割り当て */
	tsk = kmem_cache_alloc(task_struct_cachep);
	if (!tsk)
	{
		return NULL;
	}

	/* カーネルスタックを割り当て */
	stack = kmalloc(THREAD_SIZE);
	if (!stack)
	{
		kmem_cache_free(task_struct_cachep, tsk);
		return NULL;
	}

	/* task_structの内容をコピー */
	memcpy(tsk, orig, sizeof(*tsk));

	/** 新しいスタックを設定する
	 * @note スタックの値は親プロセスから引き継がない（子プロセスは新しいスタックを使うため）
	 */
	tsk->stack = stack;

	return tsk;
}

/** mm_structをコピー
 * @param tsk コピー先のtask_struct
 * @param oldmm コピー元のmm_struct
 * @return 0（成功）、負のエラーコード（失敗）
 * @note Phase 6でCOW[Copy On Write]実装予定
 */
static int copy_mm(struct task_struct *tsk, struct mm_struct *oldmm)
{
	struct mm_struct *mm;

	/* カーネルスレッド（mm == NULL）の場合はコピー不要 */
	if (!oldmm)
	{
		tsk->mm = NULL;
		return 0;
	}

	/* 新しいmm_structを割り当て */
	mm = kmalloc(sizeof(*mm));
	if (!mm)
	{
		return -ENOMEM;
	}

	/* mm_structの内容をコピー（Phase 6でページテーブルコピー実装予定） */
	memcpy(mm, oldmm, sizeof(*mm));

	/* 参照カウントを初期化 */
	mm->mm_count.counter = 1;

	tsk->mm = mm;
	return 0;
}

/** シグナル状態をコピー
 * @param tsk コピー先のtask_struct
 * @return 0（成功）、負のエラーコード（失敗）
 * @note Phase 4で詳細実装予定
 */
static int copy_signal(struct task_struct *tsk)
{
	struct signal_struct *sig;

	/* 新しいsignal_structを割り当て */
	sig = kmalloc(sizeof(*sig));
	if (!sig)
	{
		return -ENOMEM;
	}

	/* 参照カウントを初期化 */
	sig->sigcnt.counter = 1;

	tsk->signal = sig;
	return 0;
}

/** プロセスをコピー
 * @param orig コピー元のtask_struct
 * @return 新しいtask_struct（失敗時NULL）
 */
struct task_struct *copy_process(struct task_struct *orig)
{
	struct task_struct *p;
	struct pid *pid;
	int err;

	/* task_structを複製 */
	p = dup_task_struct(orig);
	if (!p)
	{
		return NULL;
	}

	/* 新しいPIDを割り当て */
	pid = alloc_pid();
	if (!pid)
	{
		if (p->stack)
		{
			kfree(p->stack);
		}
		kmem_cache_free(task_struct_cachep, p);
		return NULL;
	}
	p->pid = pid->nr;

	/* mm_structをコピー */
	err = copy_mm(p, orig->mm);
	if (err)
	{
		put_pid(pid);
		if (p->stack)
		{
			kfree(p->stack);
		}
		kmem_cache_free(task_struct_cachep, p);
		return NULL;
	}

	/* シグナル状態をコピー */
	err = copy_signal(p);
	if (err)
	{
		if (p->mm)
		{
			kfree(p->mm);
		}
		put_pid(pid);
		if (p->stack)
		{
			kfree(p->stack);
		}
		kmem_cache_free(task_struct_cachep, p);
		return NULL;
	}

	/* 親子関係を設定 */
	p->parent = orig;			  /* 親はコピー元 */
	INIT_LIST_HEAD(&p->children); /* 子リストを初期化 */
	INIT_LIST_HEAD(&p->sibling);  /* 兄弟リストを初期化 */
	INIT_LIST_HEAD(&p->tasks);	  /* グローバルリストを初期化 */

	/* コピー元の子リストに追加（親の視点では新しい子） */
	list_add_tail(&p->sibling, &orig->children);

	/* グローバルタスクリストに追加 */
	extern struct list_head task_list; /* kernel/sched/core.cのtask_list */
	list_add_tail(&p->tasks, &task_list);

	/* 新プロセスを実行可能状態に */
	p->__state = TASK_RUNNING;

	return p;
}

/** プロセスをfork
 * @return 新しいプロセスのPID（成功）、負のエラーコード（失敗）
 * @note Linux 6.18のkernel_clone()相当。Phase 10でsys_fork()から呼ばれる
 */
pid_t do_fork(void)
{
	struct task_struct *p;
	extern struct task_struct *current; /* 現在のプロセス */

	/* 現在のプロセスをコピー */
	p = copy_process(current);
	if (!p)
	{
		return -EAGAIN;
	}

	/* 新プロセスのPIDを返す */
	return p->pid;
}

/** fork初期化
 * @note カーネル起動時に呼ばれる
 */
void __init fork_init(void)
{
	/* task_struct用スラブキャッシュを作成 */
	task_struct_cachep = kmem_cache_create("task_struct", sizeof(struct task_struct));
}
