#include <asm-i386/pgtable.h>
#include <kfs/errno.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/pid.h>
#include <kfs/printk.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/string.h>

/* 初期化マクロ（Phase 1では何もしない） */
#define __init

/** task_struct用スラブキャッシュ
 * @note 頻繁に割り当て/解放されるため、スラブアロケータで高速化
 * @note exit.cからも参照されるためグローバル変数
 */
struct kmem_cache *task_struct_cachep = NULL;

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

	/* カーネルスタックを割り当て
	 * kmalloc(THREAD_SIZE) はメタデータ 8 バイト分オフセットされた ptr を返すため
	 * task->stack + THREAD_SIZE が PAGE 境界を超えて pt_regs.esp/ss を破壊する。
	 * alloc_pages は PAGE_SIZE 境界に揃った ptr を返すため安全。
	 */
	stack = (void *)alloc_pages(GFP_KERNEL, 0);
	if (!stack)
	{
		kmem_cache_free(task_struct_cachep, tsk);
		return NULL;
	}

	/* task_structの内容をコピー */
	memcpy(tsk, orig, sizeof(*tsk));

	// リスト系フィールドを初期化し，親のチェーンへのポインタを引き継がないようにする
	INIT_LIST_HEAD(&tsk->children);
	INIT_LIST_HEAD(&tsk->sibling);
	INIT_LIST_HEAD(&tsk->tasks);
	INIT_LIST_HEAD(&tsk->run_list); /* RR ランキュー用リンク初期化 */
	tsk->se.run_node.__rb_parent_color = 0;
	tsk->se.run_node.rb_right = NULL;
	tsk->se.run_node.rb_left = NULL;

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
 * @note COW（Copy On Write）はPhase 6で実装予定
 */
static int copy_mm(struct task_struct *tsk, struct mm_struct *oldmm)
{
	struct mm_struct *mm;
	pgd_t *new_pgd;
	int ret;

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

	/* mm_structのメタデータをコピー */
	memcpy(mm, oldmm, sizeof(*mm));

	/* ページテーブルを複製（子プロセスのメモリ空間を親から分離） */
	if (oldmm->pgd)
	{
		new_pgd = (pgd_t *)alloc_pages(GFP_KERNEL | GFP_ZERO, 0);
		if (!new_pgd)
		{
			kfree(mm);
			return -ENOMEM;
		}
		ret = copy_page_tables(new_pgd, oldmm->pgd);
		if (ret < 0)
		{
			free_pages((struct page *)new_pgd, 0);
			kfree(mm);
			return ret;
		}
		mm->pgd = new_pgd;
	}

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
			free_pages((struct page *)p->stack, 0);
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
			free_pages((struct page *)p->stack, 0);
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
			free_pages((struct page *)p->stack, 0);
		}
		kmem_cache_free(task_struct_cachep, p);
		return NULL;
	}

	/* コンテキストスイッチ用スタックフレームは do_fork() で設定する */

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

/** プロセスを誕生させる
 * @param user_eip 子が ring-3 で実行を開始するアドレス（0 なら親の pt_regs をコピー）
 * @param user_esp 子の ring-3 スタックポインタ（user_eip=0 なら無視）
 * @return 新しいプロセスのPID（成功）、負のエラーコード（失敗）
 * @note Linux 6.18のkernel_clone()相当。
 *       sys_fork() からは do_fork(0,0) で呼ぶ（親の pt_regs をコピー）。
 *       cmd_sched() 等からは do_fork(eip, esp) で呼ぶ（ring-3 直接起動）。
 */
pid_t do_fork(unsigned long user_eip, unsigned long user_esp)
{
	struct task_struct *p;
	extern struct task_struct *current; /* 現在のプロセス */

	/* 現在のプロセスをコピー */
	p = copy_process(current);
	if (!p)
	{
		return -EAGAIN;
	}

	/* コンテキストスイッチ用スタックフレームを設定 */
	copy_thread(p, current, user_eip, user_esp);

	/* 子プロセスをRRランキューに登録してスケジューリング可能にする */
	rr_enqueue(p);

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
