/**
 * テスト用: 共通リセット関数
 *
 * 全テストの独立性を保証するために、各テスト前に呼び出すリセット関数を提供する。
 */

#include <kfs/list.h>
#include <kfs/mm.h>
#include <kfs/pty.h>
#include <kfs/rr.h>
#include <kfs/sched.h>
#include <kfs/slab.h>
#include <kfs/vmalloc.h>

/* core.c 内部のグローバルシンボルへのアクセス */
extern struct task_struct init_task;
extern struct task_struct *current;
extern struct list_head task_list;
extern void init_idle_task(void);

/**
 * 全サブシステムを初期状態にリセット
 * @details
 * 全テストで統一的に使用するリセット関数。
 * テストの独立性を保証するために、各テスト前に呼び出す。
 *
 * リセット対象:
 * - 仮想メモリ領域（VMA）
 * - ページアロケータ
 * - Slabアロケータ
 * - vmallocアロケータ
 */
void reset_all_state_for_test(void)
{
	/* VMAをリセット（依存関係: Slabの前にクリア） */
	vm_reset_for_test();

	/* ページアロケータをリセット */
	page_allocator_reset_for_test();

	/* Slabアロケータをリセット（ページアロケータに依存） */
	kmem_cache_reset_for_test();

	/* vmallocアロケータを初期化（Slabアロケータに依存） */
	vmalloc_init();
	pty_reset();

	/* スケジューラ状態をリセット（init_task.run_list の不整合を防ぐ） */
	/* グローバルタスクリストをクリア */
	INIT_LIST_HEAD(&task_list);
	/* init_task の各リストをリセット */
	INIT_LIST_HEAD(&init_task.children);
	INIT_LIST_HEAD(&init_task.sibling);
	INIT_LIST_HEAD(&init_task.tasks);
	/* run_list を空にしないと rr_enqueue の二重登録防止チェックが誤作動する */
	INIT_LIST_HEAD(&init_task.run_list);
	/* init_task の状態を起動直後に戻す */
	init_task.__state = TASK_RUNNING;
	init_task.flags = PF_KTHREAD;
	init_task.policy = SCHED_PURE_RR;
	init_task.prio = DEFAULT_PRIO;
	init_task.static_prio = DEFAULT_PRIO;
	init_task.nice = 0;
	init_task.rt_priority = 0;
	init_task.time_slice = RR_TIMESLICE;
	init_task.se.load = NICE_0_LOAD;
	init_task.se.on_rq = 0;
	init_task.se.vruntime = 0;
	init_task.se.run_node.__rb_parent_color = 0;
	init_task.se.run_node.rb_left = NULL;
	init_task.se.run_node.rb_right = NULL;
	/* thread.sp=0 にして「cpu_idle_loop がまだ動いていない」状態にする。
	 * schedule() の init_task フォールバックはこれが 0 の間は無効になる。 */
	init_task.thread.sp = 0;
	init_task.pid = 0;
	init_task.parent = &init_task;
	current = &init_task;
	/* RR ランキューをクリアして init_task を再登録 */
	init_idle_task();
	sched_init();
}
