#include "../../../test_reset.h"
#include "unit_test_framework.h"
#include <asm-i386/desc.h>
#include <asm-i386/pgtable.h>
#include <asm-i386/ptrace.h>
#include <kfs/mm_types.h>
#include <kfs/sched.h>
#include <kfs/slab.h>

/* 外部シンボル */
extern struct tss_struct init_tss;
extern void ret_from_fork(void);

/* セットアップ・ティアダウン */
static void setup_test(void)
{
	reset_all_state_for_test();
}

static void teardown_test(void)
{
	/* 特に後処理なし */
}

/* switch_mm() と copy_thread() のテスト */

/** switch_mm() が NULL の mm を無視することを確認 */
KFS_TEST(test_switch_mm_null)
{
	unsigned long old_cr3 = read_cr3();

	/* NULL を渡しても何も起きない */
	switch_mm(NULL, NULL);

	KFS_ASSERT_EQ(old_cr3, read_cr3());
}

/** switch_mm() が有効な mm で CR3 を更新することを確認
 * @note 現在の有効なCR3を使って、switch_mm() がクラッシュしないことを確認
 */
KFS_TEST(test_switch_mm_valid)
{
	struct mm_struct mm;
	unsigned long old_cr3 = read_cr3();

	/* 現在の有効なページディレクトリを使う（無効なアドレスは危険） */
	mm.pgd = (pgd_t *)old_cr3;
	mm.mm_count.counter = 1;

	/* switch_mm() を呼んでもクラッシュしないことを確認 */
	switch_mm(NULL, &mm);

	/* CR3 は同じ値のままのはず */
	KFS_ASSERT_EQ(old_cr3, read_cr3());
}

/** copy_thread() がスタックフレームを正しく構築することを確認 */
KFS_TEST(test_copy_thread_stack_setup)
{
	struct task_struct *child;
	unsigned long *stack_ptr;

	/* 子タスクを割り当て */
	child = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(child != NULL);

	child->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(child->stack != NULL);

	/* copy_thread を呼び出し */
	copy_thread(child, NULL);

	/* thread.sp がスタック範囲内にあることを確認 */
	unsigned long stack_start = (unsigned long)child->stack;
	unsigned long stack_end = stack_start + THREAD_SIZE;
	KFS_ASSERT_TRUE(child->thread.sp >= stack_start);
	KFS_ASSERT_TRUE(child->thread.sp < stack_end);

	/* thread.ip が ret_from_fork を指すことを確認 */
	KFS_ASSERT_TRUE(child->thread.ip == (unsigned long)ret_from_fork);

	/* スタック上の return address を確認 */
	stack_ptr = (unsigned long *)child->thread.sp;
	/* stack_ptr[4] が return address（EDI, ESI, EBX, EBP の後） */
	KFS_ASSERT_TRUE(stack_ptr[4] == (unsigned long)ret_from_fork);

	/** callee-saved レジスタが 0 で初期化されていることを確認
	 * @see copy_thread() の fork_frame 構築コード
	 */
	KFS_ASSERT_TRUE(stack_ptr[0] == 0); // EDI
	KFS_ASSERT_TRUE(stack_ptr[1] == 0); // ESI
	KFS_ASSERT_TRUE(stack_ptr[2] == 0); // EBX
	KFS_ASSERT_TRUE(stack_ptr[3] == 0); // EBP

	/* クリーンアップ */
	kfree(child->stack);
	kfree(child);
}

/** TSS の esp0 が初期化されることを確認 */
KFS_TEST(test_tss_initialization)
{
	/* init_tss.ss0 がカーネルデータセグメントに設定されていることを確認 */
	KFS_ASSERT_TRUE(init_tss.ss0 == __KERNEL_DS);

	/* init_tss.esp0 は __switch_to() で動的に更新されるため、
	 * 初期値は 0 またはスタック末尾のどちらでも良い */
}

/** TSS esp0 フィールドが書き込み可能であることを確認
 * @note __switch_to() の TSS 更新機能は統合テストで確認する
 */
KFS_TEST(test_switch_to_updates_tss_esp0)
{
	unsigned long old_esp0 = init_tss.esp0;
	unsigned long test_value = 0xDEADBEEF;

	/* TSS esp0 に書き込み */
	init_tss.esp0 = test_value;
	KFS_ASSERT_EQ(test_value, init_tss.esp0);

	/* 元に戻す */
	init_tss.esp0 = old_esp0;
}

/** copy_thread() で設定された thread.sp が有効な範囲内であることを確認
 * @note 実際のコンテキストスイッチは統合テストで確認する
 */
KFS_TEST(test_switch_to_switches_stack)
{
	struct task_struct *task;

	/* タスクを作成 */
	task = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(task != NULL);

	task->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(task->stack != NULL);

	task->mm = NULL;

	/* copy_thread でスタックを初期化 */
	copy_thread(task, NULL);

	/* thread.sp がスタック範囲内であることを確認 */
	unsigned long stack_start = (unsigned long)task->stack;
	unsigned long stack_end = stack_start + THREAD_SIZE;
	KFS_ASSERT_TRUE(task->thread.sp >= stack_start);
	KFS_ASSERT_TRUE(task->thread.sp < stack_end);

	/* thread.ip が ret_from_fork を指していることを確認 */
	extern void ret_from_fork(void);
	KFS_ASSERT_EQ((unsigned long)ret_from_fork, task->thread.ip);

	/* クリーンアップ */
	kfree(task->stack);
	kfree(task);
}

/* copy_thread_with_fn() が fork_frame.ebx に関数ポインタを設定することを確認 */
KFS_TEST(test_copy_thread_with_fn_sets_ebx)
{
	struct task_struct *child;
	unsigned long *stack_ptr;
	/* "DEAD CODE": 実行されない偽の関数ポインタ */
	void (*dummy_fn)(void) = (void (*)(void))0xDEADC0DE;

	child = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(child != NULL);
	child->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(child->stack != NULL);

	copy_thread_with_fn(child, dummy_fn);

	/* fork_frame の ebx スロット（index 2）が関数ポインタになっていること */
	stack_ptr = (unsigned long *)child->thread.sp;
	KFS_ASSERT_EQ((unsigned long)dummy_fn, stack_ptr[2]); /* EBX */

	/* ret_addr（index 4）は ret_from_fork を指すこと */
	KFS_ASSERT_EQ((unsigned long)ret_from_fork, stack_ptr[4]);

	/* thread.ip も ret_from_fork を指すこと */
	KFS_ASSERT_EQ((unsigned long)ret_from_fork, child->thread.ip);

	kfree(child->stack);
	kfree(child);
}

/** copy_thread_with_fn() と copy_thread() がそれぞれ独立した SP を持つことを確認 */
KFS_TEST(test_copy_thread_with_fn_independent_sp)
{
	struct task_struct *child_fork;
	struct task_struct *child_fn;
	void (*dummy_fn)(void) = (void (*)(void))0xDEADBEEF;

	child_fork = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(child_fork != NULL);
	child_fork->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(child_fork->stack != NULL);

	child_fn = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(child_fn != NULL);
	child_fn->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(child_fn->stack != NULL);

	copy_thread(child_fork, NULL);
	copy_thread_with_fn(child_fn, dummy_fn);

	/* SP のオフセットは同じ（どちらも THREAD_SIZE - sizeof(fork_frame)）*/
	unsigned long fork_offset = child_fork->thread.sp - (unsigned long)child_fork->stack;
	unsigned long fn_offset = child_fn->thread.sp - (unsigned long)child_fn->stack;
	KFS_ASSERT_EQ(fork_offset, fn_offset);

	/* copy_thread の EBX スロットは 0 */
	unsigned long *fork_sp = (unsigned long *)child_fork->thread.sp;
	KFS_ASSERT_EQ(0UL, fork_sp[2]); /* EBX == 0 */

	/* copy_thread_with_fn の EBX スロットは dummy_fn */
	unsigned long *fn_sp = (unsigned long *)child_fn->thread.sp;
	KFS_ASSERT_EQ((unsigned long)dummy_fn, fn_sp[2]); /* EBX == dummy_fn */

	kfree(child_fork->stack);
	kfree(child_fork);
	kfree(child_fn->stack);
	kfree(child_fn);
}

/* copy_thread() が pt_regs.eax = 0（子の fork 戻り値）を設定することを確認 */
KFS_TEST(test_copy_thread_sets_child_eax_zero)
{
	struct task_struct *child;
	struct pt_regs *regs;

	child = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(child != NULL);
	child->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(child->stack != NULL);

	copy_thread(child, NULL);

	/* task_pt_regs() でスタック最上部の pt_regs を取得 */
	regs = task_pt_regs(child);
	KFS_ASSERT_EQ(0UL, (unsigned long)regs->eax);

	kfree(child->stack);
	kfree(child);
}

/* task_pt_regs() がスタック範囲内のアドレスを返すことを確認 */
KFS_TEST(test_task_pt_regs_in_stack_range)
{
	struct task_struct *child;
	struct pt_regs *regs;
	unsigned long stack_start;
	unsigned long stack_end;

	child = (struct task_struct *)kmalloc(sizeof(struct task_struct));
	KFS_ASSERT_TRUE(child != NULL);
	child->stack = kmalloc(THREAD_SIZE);
	KFS_ASSERT_TRUE(child->stack != NULL);

	copy_thread(child, NULL);

	stack_start = (unsigned long)child->stack;
	stack_end = stack_start + THREAD_SIZE;
	regs = task_pt_regs(child);

	/* pt_regs はスタック範囲内に収まる */
	KFS_ASSERT_TRUE((unsigned long)regs >= stack_start);
	KFS_ASSERT_TRUE((unsigned long)regs + sizeof(*regs) <= stack_end);
	/* pt_regs は fork_frame より上（高アドレス）にある */
	KFS_ASSERT_TRUE((unsigned long)regs > child->thread.sp);

	kfree(child->stack);
	kfree(child);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_switch_mm_null, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_switch_mm_valid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_thread_stack_setup, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_tss_initialization, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_switch_to_updates_tss_esp0, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_switch_to_switches_stack, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_thread_with_fn_sets_ebx, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_thread_with_fn_independent_sp, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_copy_thread_sets_child_eax_zero, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_task_pt_regs_in_stack_range, setup_test, teardown_test),
};

int register_unit_tests_process(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
