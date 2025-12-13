#include <kfs/list.h>
#include <kfs/mm_types.h>
#include <kfs/pid.h>
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

	/* プロセス階層 */
	.parent = &init_task,							/* 自分自身が親 */
	.children = LIST_HEAD_INIT(init_task.children), /* 子リスト */
	.sibling = LIST_HEAD_INIT(init_task.sibling),	/* 兄弟リスト */

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

	/* スケジューリング */
	.se =
		{
			.load = 0,
			.run_node = {0},
			.on_rq = 0,
			.vruntime = 0,
		},

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
static LIST_HEAD(task_list);

/** init_taskの最終初期化
 * @brief init_taskの静的初期化できない部分を実行時に初期化する．
 *        これにより，init_taskは完全に初期化される．
 * @note init_taskの静的初期化は変数宣言とともに実施済
 */
void init_idle_task(void)
{
	/* init_taskをタスクリストに追加（最初のタスク） */
	list_add(&init_task.sibling, &task_list);

	/* 現在のタスクとして設定 */
	current = &init_task;
}
