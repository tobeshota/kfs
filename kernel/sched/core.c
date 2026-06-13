#include <kfs/errno.h>
#include <kfs/list.h>
#include <kfs/mm_types.h>
#include <kfs/pid.h>
#include <kfs/printk.h>
#include <kfs/rr.h>
#include <kfs/sched.h>

/** idle/swapperプロセス (PID=0)
 * @details すべてのプロセスの祖先．静的に定義され，カーネル起動時に実行される最初のプロセス．
 *          fork()でinitプロセス(PID=1)を作成し，その後は実行可能なプロセスがない時にCPUをアイドル状態にする．
 */
struct task_struct init_task = {
	/* 状態管理 */
	.__state = TASK_RUNNING, /* 実行可能状態 */
	.stack = NULL,			 /* カーネル初期スタック使用 */
	.flags = PF_KTHREAD,	 /* カーネルスレッド */

	/* メモリ管理（カーネルスレッドなのでNULL） */
	.mm = NULL, /* ユーザー空間なし */

	/* プロセスID */
	.pid = 0, /* PID 0（idle） */

	/* プロセスグループ／セッション */
	.pgrp = 0,
	.session = 0,
	.tty_console = 0,

	/* プロセス階層 */
	.parent = &init_task,							/* 自分自身が親 */
	.children = LIST_HEAD_INIT(init_task.children), /* 子リスト */
	.sibling = LIST_HEAD_INIT(init_task.sibling),	/* 兄弟リスト */
	.tasks = LIST_HEAD_INIT(init_task.tasks),		/* グローバルタスクリスト */
	.run_list = LIST_HEAD_INIT(init_task.run_list), /* RRランキュー */

	/* 所有者・権限（root権限） */
	.uid = {.val = 0},			   /* root UID */
	.euid = {.val = 0},			   /* root実効UID */
	.cap_effective = CAP_FULL_SET, /* 全Capability有効 */

	/* シグナル（後で初期化） */
	.signal = NULL, /* Phase 4で設定 */
	.pending =
		{
			.list = LIST_HEAD_INIT(init_task.pending.list),
			.signal = 0,
		},

	/* スケジューリング（CFS用エンティティ） */
	.se =
		{
			.load = 0,
			.run_node = {0},
			.on_rq = 0,
			.vruntime = 0,
		},

	/* スケジューリングポリシー（Phase 7） */
	.policy = SCHED_PURE_RR, /* 初期ポリシーは純粋ラウンドロビン（Phase 8でSCHED_NORMALに変更） */
	.prio = 20,				 /* デフォルト優先度 */
	.rt_priority = 0,
	.time_slice = 10, /* RR_TIMESLICE（kernel/sched/rr.c で定義） */
	.cpu_time_ticks = 0,

	/* プロセス名 */
	.comm = "swapper", /* idle/swapperプロセス */
};

/** 現在実行中のタスクへのポインタ
 * @note 単一CPUなので通常のグローバル変数として実装
 */
struct task_struct *current = &init_task;

/** 全タスクのリスト
 * 全てのtask_structをつなぐグローバルリスト
 * Phase 2でタスク検索等に使用
 */
LIST_HEAD(task_list);

/** init_taskの最終初期化
 * @brief init_taskの静的初期化できない部分を実行時に初期化する．
 *        これにより，init_taskは完全に初期化される．
 * @note init_taskの静的初期化は変数宣言とともに実施済
 */
void init_idle_task(void)
{
	/* 既に初期化済みならスキップ（冪等性を保証） */
	if (!list_empty(&task_list))
	{
		return;
	}

	/* init_taskをタスクリストに追加（最初のタスク） */
	list_add(&init_task.tasks, &task_list);

	/* 現在のタスクとして設定 */
	current = &init_task;
}

/** PIDからtask_structを検索
 * @param pid 検索するプロセスID
 * @return 見つかったtask_struct（見つからない場合NULL）
 * @note グローバルタスクリストを線形探索（Phase 1-6の簡易実装）
 *       Phase 7以降でPIDハッシュテーブルによる高速化を実装予定
 */
struct task_struct *find_task_by_pid(pid_t pid)
{
	struct task_struct *task;

	/* リストが空の場合は即座にNULLを返す */
	if (list_empty(&task_list))
	{
		return NULL;
	}

	/* タスクリストを走査（tasksフィールドを使用） */
	list_for_each_entry(task, &task_list, tasks)
	{
		if (task->pid == pid)
		{
			return task;
		}
	}

	return NULL;
}

/** 全タスクを走査する
 * @param fn 各 task_struct に対するコールバック
 * @param ctx コールバックへ渡す任意データ
 * @return 0=全件走査完了, 負数=エラー, 正数=コールバックが返した中断コード
 */
int task_for_each(int (*fn)(struct task_struct *task, void *ctx), void *ctx)
{
	struct task_struct *task;

	if (!fn)
	{
		return -EINVAL;
	}

	list_for_each_entry(task, &task_list, tasks)
	{
		int ret = fn(task, ctx);
		if (ret)
		{
			return ret;
		}
	}

	return 0;
}

/** スケジューラを初期化する
 * @brief RR サブスケジューラを初期化する．
 *        init_task は RR キューに入れない（cpu_idle_loop() のフォールバック先として扱う）．
 *        init/main.c の start_kernel() から呼び出す．
 */
void sched_init(void)
{
	INIT_LIST_HEAD(&init_task.run_list);
	rr_init();
	/* init_task は RR キューに登録しない。
	 * schedule() が rr_pick_next()==NULL のとき init_task へフォールバックする。
	 * thread.sp は cpu_idle_loop() 内で最初に __switch_to が走った瞬間に
	 * 自動保存されるため、ここでは 0 のままにしておく。
	 * 0 の間は fallback を無効化することでテスト環境での誤スイッチを防ぐ。 */
	init_task.thread.sp = 0;
	current = &init_task;
}

/** プロセスを起床させる
 * @brief プロセスの状態をTASK_RUNNINGに変更し，RRランキューに追加する．
 *        これにより，プロセスは次回のスケジューリングにおいて実行対象の候補となる．
 * @param tsk 起床させるプロセス
 */
void wake_up_process(struct task_struct *tsk)
{
	tsk->__state = TASK_RUNNING;
	rr_enqueue(tsk);
}

/** タイマーティックハンドラから呼ばれる周期処理
 * @brief 現在のタスクのタイムスライスをデクリメントし，必要に応じてプリエンプトする．
 *        arch/i386/kernel/timer.c の timer_interrupt() から呼び出す（Phase 4 コミットで実装）．
 */
void scheduler_tick(void)
{
	if (!(current->flags & PF_KTHREAD))
	{
		/** ユーザープロセスの場合のみ CPU 時間をカウントする
		 * @note ユーザプロセスのみに絞る理由は，
		 *       CPU時間はユーザプロセスのリソース使用量の指標であり，
		 *       カーネルスレッドは通常システム管理やバックグラウンドタスクであり，
		 *       CPU時間をカウントする必要がないためである．
		 */
		current->cpu_time_ticks++;
	}
	rr_task_tick(current);
}

/** アイドルループ
 * @brief init_task のメイン関数．
 *        実行可能なタスクがないとき CPU を hlt で休止し，
 *        タイマー割り込みで目覚めたら schedule() でランキューを回す．
 *        欲湬のバックグラウンドプロセスが EXIT_ZOMBIE になったら reap する。
 * @note この関数から戻ることはない。
 */
__attribute__((weak, noreturn)) void cpu_idle_loop(void)
{
	while (1)
	{
		/* タイマー割り込みを待つ */
		__asm__ volatile("hlt");

		/* 起きたら他タスクへスイッチ */
		schedule();
	}
	__builtin_unreachable();
}

/** スケジューラ本体（コンテキストスイッチ）
 * @brief RR ランキューから次のタスクを選択し current ポインタを更新する．
 *        自発的に呼ばれた場合（do_wait等）は current をランキュー末尾に回して他タスクを先頭に立てる。
 * @return 1=コンテキストスイッチ実施, 0=スイッチなし（init_task から呼ばれた等）
 * @note hlt は cpu_idle_loop() 内のみで行う。
 */
int schedule(void)
{
	struct task_struct *next;
	struct task_struct *prev = current;

	/* current をランキューの末尾に回して他タスクが先頭に来られるようにする。
	 * ランキューにない場合（TASK_DEAD 等）は何もしない。
	 * TASK_RUNNING のときだけ末尾に再挿入する。
	 * TASK_INTERRUPTIBLE / TASK_UNINTERRUPTIBLE は wake_up_process() が
	 * 呼ばれるまでランキューに戻さない。 */
	if (!list_empty(&prev->run_list))
	{
		rr_dequeue(prev);
		if (prev->__state == TASK_RUNNING)
		{
			rr_enqueue(prev);
		}
	}

	next = rr_pick_next();

	/* RR キューが空、または prev 以外に runnable なタスクがない
	 * → init_task（cpu_idle_loop）へフォールバック */
	if (!next || next == prev)
	{
		/* すでに init_task が動いている → スイッチ不要 */
		if (prev == &init_task)
		{
			return 0; /* init_task.thread.sp == 0 は cpu_idle_loop() がまだ起動していない
					   * （テスト環境・起動直後）ことを意味する。スイッチしない。 */
		}
		if (!init_task.thread.sp)
		{
			return 0;
		}
		current = &init_task;
		__switch_to(prev, &init_task);
		return 0;
	}

	/* next->thread.sp == 0 は copy_thread() が未呼び出しで
	 * カーネルスタックフレームが未設定であることを意味する。
	 * （do_fork() を経ずに作られたタスク等）
	 * ESP=0 で __switch_to するとトリプルフォールトするためスキップする。 */
	if (!next->thread.sp)
	{
		return 0;
	}

	current = next;
	__switch_to(prev, next);
	return 1; /* スイッチ実施 */
}
