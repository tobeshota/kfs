#include <asm-i386/pgtable.h>
#include <kfs/errno.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/mman.h>
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

/** 現在生存しているスレッド（プロセス）数
 * @note copy_process() で ++ され，
 *       release_task() で -- される
 */
int nr_threads = 0;

/** fork bomb 防止の上限
 * @note fork_init() で total_pages を元に計算される
 */
int max_threads;

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

	/* ユーザスタックは子プロセス固有に設定するため親の値を引き継がない
	 * do_fork() / kernel_thread() が必要に応じて設定する */
	tsk->user_stack_vm_start = 0;
	tsk->user_stack_vm_len = 0;

	return tsk;
}

/** mm_structをコピー
 * @param tsk コピー先のtask_struct
 * @param oldmm コピー元のmm_struct
 * @return 0（成功）、負のエラーコード（失敗）
 * @note COW（Copy On Write）はPhase 6で実装予定
 */

/* リンカが生成するセクション境界シンボル（linker.ld で定義） */
extern unsigned long __text_start;
extern unsigned long __text_end;
extern unsigned long __data_start;
extern unsigned long __data_end;
extern unsigned long __bss_start;
extern unsigned long __bss_end;

/** カーネルセクション境界を mm_struct に記録する
 * @brief copy_mm() 内のカーネルコンテキストから呼ばれる。
 *        Linux では binfmt_elf.c の load_elf_binary() が ELF ヘッダから設定するが、
 *        kfs には ELF ローダがなく全プロセスが同一バイナリを共有するため
 *        linker シンボルで静的に設定する。
 */
static void mm_set_kernel_sections(struct mm_struct *mm)
{
	if (!mm)
	{
		return;
	}
	mm->start_code = (unsigned long)&__text_start;
	mm->end_code = (unsigned long)&__text_end;
	mm->start_data = (unsigned long)&__data_start;
	mm->end_data = (unsigned long)&__data_end;
	mm->start_bss = (unsigned long)&__bss_start;
	mm->end_bss = (unsigned long)&__bss_end;
}

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

	/* BSS/data/text セクション境界を設定 */
	mm_set_kernel_sections(mm);

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

	/* fork bomb 防止：スレッド上限チェック */
	if (nr_threads >= max_threads)
	{
		return NULL;
	}

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
	nr_threads++;

	/* 新プロセスを実行可能状態に */
	p->__state = TASK_RUNNING;

	return p;
}

/** プロセスを誕生させる
 * @param user_eip 子が ring-3 で実行を開始するアドレス（0 なら親の pt_regs をコピー）
 * @return 新しいプロセスのPID（成功）、負のエラーコード（失敗）
 * @note Linux 6.18のkernel_clone()相当。
 *       sys_fork() からは do_fork(0) で呼ぶ（親の pt_regs をコピー）。
 *       cmd_sched() 等からは do_fork(eip) で呼ぶ（ring-3 直接起動）。
 *       ユーザスタックは内部で do_mmap(MAP_ANONYMOUS) により動的確保する。
 */
pid_t do_fork(unsigned long user_eip)
{
	struct task_struct *p;
	extern struct task_struct *current; /* 現在のプロセス */
	unsigned long user_esp = 0;
	void *ustack = MAP_FAILED;
	const unsigned long STACK_SIZE = PAGE_SIZE; /* 4KB */

	/* user_eip が指定された場合はユーザスタックを動的確保 */
	if (user_eip != 0)
	{
		ustack = do_mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
		if (ustack == MAP_FAILED)
		{
			printk(KERN_WARNING "do_fork: failed to allocate user stack\n");
			return -ENOMEM;
		}
		/* スタックはアドレス高位から使うため末尾を渡す */
		user_esp = (unsigned long)ustack + STACK_SIZE;
	}
	else if (current->user_stack_vm_start != 0)
	{
		/* sys_fork() 経由（user_eip=0）かつ親がユーザスタックを持つ場合は
		 * 新しい物理ページを確保して内容をコピーする。
		 * copy_page_tables() はページテーブルのみコピーし物理ページは共有するため、
		 * 親が exit() して do_munmap() すると子のスタックが解放されてしまう。 */
		unsigned long src_start = current->user_stack_vm_start;
		unsigned long src_size = current->user_stack_vm_len ? current->user_stack_vm_len : STACK_SIZE;

		ustack = do_mmap(NULL, src_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
		if (ustack == MAP_FAILED)
		{
			printk(KERN_WARNING "do_fork: failed to copy user stack\n");
			return -ENOMEM;
		}
		/* 親のスタック内容をコピー（shallow copy で十分: スタックデータをそのまま複製） */
		memcpy(ustack, (void *)src_start, src_size);
		/* user_esp は親の pt_regs を copy_thread がコピーするため変更不要。
		 * ただし子の仮想アドレスは ustack から始まるため，
		 * ESP のオフセット（src_esp - src_start）を新アドレス系に変換して渡す。
		 * → copy_thread(user_eip=0) は親の pt_regs をそのまま使うため
		 *   copy_thread 後に childregs->esp だけ付け替える必要がある。
		 * ここでは user_esp_new を計算し copy_thread の後に適用する。 */
	}

	/* 現在のプロセスをコピー */
	p = copy_process(current);
	if (!p)
	{
		if (ustack != MAP_FAILED)
		{
			unsigned long sz = current->user_stack_vm_len ? current->user_stack_vm_len : STACK_SIZE;
			do_munmap((unsigned long)ustack, sz);
		}
		return -EAGAIN;
	}

	/* 子プロセスにユーザスタック情報を記録（exit 時に do_munmap で解放するため） */
	if (ustack != MAP_FAILED)
	{
		unsigned long sz =
			(user_eip != 0) ? STACK_SIZE : (current->user_stack_vm_len ? current->user_stack_vm_len : STACK_SIZE);
		p->user_stack_vm_start = (unsigned long)ustack;
		p->user_stack_vm_len = sz;
	}

	/* コンテキストスイッチ用スタックフレームを設定 */
	copy_thread(p, current, user_eip, user_esp);

	/* sys_fork() 経由でスタックをコピーした場合：
	 * copy_thread は親の pt_regs.esp をそのまま子にコピーしているが、
	 * 子の新しいスタック仮想アドレスに ESP を付け替える */
	if (user_eip == 0 && ustack != MAP_FAILED && current->user_stack_vm_start != 0)
	{
		struct pt_regs *childregs;
		unsigned long src_start = current->user_stack_vm_start;
		unsigned long src_size = p->user_stack_vm_len;
		unsigned long old_esp;
		unsigned long new_esp;

		childregs = task_pt_regs(p);
		old_esp = childregs->esp;
		/* ESP のスタック先頭からのオフセットを保存し，新アドレス系でオフセットを再現 */
		if (old_esp >= src_start && old_esp < src_start + src_size)
		{
			new_esp = (unsigned long)ustack + (old_esp - src_start);
		}
		else
		{
			/* フォールバック: 新スタックトップ */
			new_esp = (unsigned long)ustack + src_size;
		}
		childregs->esp = new_esp;

		/* EBP チェーンも旧アドレス系 → 新アドレス系に変換する。
		 * memcpy でスタック内容をコピーしても saved EBP は旧スタック範囲
		 * (src_start ~ src_start+src_size) を指したままのため、
		 * *pte = 0 で旧 PTE をクリアした後に孫が EBP 経由でアクセスすると
		 * ページフォルトが発生する。スタックを walk して旧→新に変換する。 */
		unsigned long ebp = childregs->ebp;
		unsigned long delta = (unsigned long)ustack - src_start;
		int depth = 0;
		/* childregs->ebp が旧スタック内なら新アドレスに変換 */
		if (ebp >= src_start && ebp < src_start + src_size)
		{
			childregs->ebp = ebp + delta;
			ebp = childregs->ebp;
			/* EBP チェーンを辿って全 saved EBP を変換 */
			while (depth < 64)
			{
				unsigned long *saved_ebp_ptr = (unsigned long *)ebp;
				unsigned long saved_ebp = *saved_ebp_ptr;
				if (saved_ebp < src_start || saved_ebp >= src_start + src_size)
				{
					break;
				}
				*saved_ebp_ptr = saved_ebp + delta;
				ebp = *saved_ebp_ptr;
				depth++;
			}
		}
	}

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

	/* fork bomb 防止の上限を計算
	 * カーネルスタックには物理メモリの 1/64 までしか使わせない。
	 * 残りは task_struct・mm_struct・ページテーブル等に確保する。 */
	extern unsigned long total_pages;
	unsigned long stack_pages_per_thread = THREAD_SIZE / PAGE_SIZE; /* 1スレッドのスタックに必要なページ数 */
	unsigned long max_stack_pages = total_pages / 64; /* スタックに使ってよい最大ページ数 */
	max_threads = (int)(max_stack_pages / stack_pages_per_thread); /* 上限スレッド数 */
	if (max_threads < 1)
	{
		max_threads = 1;
	}
}

/** 指定した関数をカーネル空間のプロセスとして実行する
 * @brief copy_thread_with_fn() が fork_frame.ebx = fn を設定することで
 *        ret_from_fork がカーネルスレッドパス（call *%%ebx）へ分岐する。
 * @param fn 新プロセスで実行するカーネル関数
 * @return 子PID（成功）、負数（失敗）
 */
pid_t kernel_thread(void (*fn)(void))
{
	extern void copy_thread_with_fn(struct task_struct * p, void (*fn)(void));
	struct task_struct *p;

	p = copy_process(current);
	if (!p)
	{
		return -EAGAIN;
	}

	/* fork_frame.ebx = fn を設定
	 * これにより，ret_from_fork がカーネルスレッドパスを選択する */
	copy_thread_with_fn(p, fn);

	/* PF_KTHREAD を明示的に設定する */
	p->flags |= PF_KTHREAD;

	/* RR ランキューに登録してスケジューリング可能にする */
	rr_enqueue(p);

	return p->pid;
}
