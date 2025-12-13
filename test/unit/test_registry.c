#include "unit_test_framework.h"

// 各テストファイルで提供される register_* 関数
int register_unit_tests_terminal_scroll(struct kfs_test_case **out);
int register_unit_tests_start_kernel(struct kfs_test_case **out);
int register_unit_tests_string(struct kfs_test_case **out);
int register_unit_tests_page_alloc(struct kfs_test_case **out);
int register_unit_tests_shell(struct kfs_test_case **out);
int register_unit_tests_keyboard(struct kfs_test_case **out);
int register_unit_tests_printk(struct kfs_test_case **out);
int register_unit_tests_stacktrace(struct kfs_test_case **out);
int register_unit_tests_memory(struct kfs_test_case **out);
int register_unit_tests_slab(struct kfs_test_case **out);
int register_unit_tests_vmalloc(struct kfs_test_case **out);
int register_unit_tests_pgtable(struct kfs_test_case **out);
int register_unit_tests_traps(struct kfs_test_case **out);
int register_unit_tests_i8259(struct kfs_test_case **out);
int register_unit_tests_irq(struct kfs_test_case **out);
int register_unit_tests_signal(struct kfs_test_case **out);
int register_unit_tests_syscall(struct kfs_test_case **out);
int register_unit_tests_sched_core(struct kfs_test_case **out);
int register_unit_tests_pid(struct kfs_test_case **out);
int register_unit_tests_rbtree(struct kfs_test_case **out);

#define KFS_MAX_TESTS 256

// すべてのテストケースを一つにまとめる
static struct kfs_test_case *all_cases = 0;
static int all_count = 0;

int register_unit_tests(struct kfs_test_case **out)
{
	// 初回だけ収集
	if (!all_cases)
	{
		struct kfs_test_case *cases_term_scroll = 0;
		int count_term_scroll = register_unit_tests_terminal_scroll(&cases_term_scroll);
		struct kfs_test_case *cases_kernel = 0;
		int count_kernel = register_unit_tests_start_kernel(&cases_kernel);
		struct kfs_test_case *cases_printk = 0;
		int count_printk = register_unit_tests_printk(&cases_printk);
		struct kfs_test_case *cases_keyboard = 0;
		int count_keyboard = register_unit_tests_keyboard(&cases_keyboard);
		struct kfs_test_case *cases_string = 0;
		int count_string = register_unit_tests_string(&cases_string);
		struct kfs_test_case *cases_shell = 0;
		int count_shell = register_unit_tests_shell(&cases_shell);
		struct kfs_test_case *cases_page_alloc = 0;
		int count_page_alloc = register_unit_tests_page_alloc(&cases_page_alloc);
		struct kfs_test_case *cases_stacktrace = 0;
		int count_stacktrace = register_unit_tests_stacktrace(&cases_stacktrace);
		struct kfs_test_case *cases_memory = 0;
		int count_memory = register_unit_tests_memory(&cases_memory);
		struct kfs_test_case *cases_slab = 0;
		int count_slab = register_unit_tests_slab(&cases_slab);
		struct kfs_test_case *cases_vmalloc = 0;
		int count_vmalloc = register_unit_tests_vmalloc(&cases_vmalloc);
		struct kfs_test_case *cases_pgtable = 0;
		int count_pgtable = register_unit_tests_pgtable(&cases_pgtable);
		struct kfs_test_case *cases_traps = 0;
		int count_traps = register_unit_tests_traps(&cases_traps);
		struct kfs_test_case *cases_i8259 = 0;
		int count_i8259 = register_unit_tests_i8259(&cases_i8259);
		struct kfs_test_case *cases_irq = 0;
		int count_irq = register_unit_tests_irq(&cases_irq);
		struct kfs_test_case *cases_signal = 0;
		int count_signal = register_unit_tests_signal(&cases_signal);
		struct kfs_test_case *cases_syscall = 0;
		int count_syscall = register_unit_tests_syscall(&cases_syscall);
		struct kfs_test_case *cases_sched_core = 0;
		int count_sched_core = register_unit_tests_sched_core(&cases_sched_core);
		struct kfs_test_case *cases_pid = 0;
		int count_pid = register_unit_tests_pid(&cases_pid);
		struct kfs_test_case *cases_rbtree = 0;
		int count_rbtree = register_unit_tests_rbtree(&cases_rbtree);
		// 動的確保は避け、静的最大数 (今は少数) を想定してスタック上に置けないので静的配列
		static struct kfs_test_case merged[KFS_MAX_TESTS];
		int idx = 0;
		for (int i = 0; i < count_term_scroll && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_term_scroll[i];
		}
		for (int i = 0; i < count_kernel && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_kernel[i];
		}
		for (int i = 0; i < count_printk && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_printk[i];
		}
		for (int i = 0; i < count_keyboard && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_keyboard[i];
		}
		for (int i = 0; i < count_string && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_string[i];
		}
		for (int i = 0; i < count_shell && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_shell[i];
		}
		for (int i = 0; i < count_page_alloc && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_page_alloc[i];
		}
		for (int i = 0; i < count_stacktrace && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_stacktrace[i];
		}
		for (int i = 0; i < count_memory && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_memory[i];
		}
		for (int i = 0; i < count_slab && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_slab[i];
		}
		for (int i = 0; i < count_vmalloc && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_vmalloc[i];
		}
		for (int i = 0; i < count_pgtable && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_pgtable[i];
		}
		for (int i = 0; i < count_traps && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_traps[i];
		}
		for (int i = 0; i < count_i8259 && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_i8259[i];
		}
		for (int i = 0; i < count_irq && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_irq[i];
		}
		for (int i = 0; i < count_signal && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_signal[i];
		}
		for (int i = 0; i < count_syscall && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_syscall[i];
		}
		for (int i = 0; i < count_sched_core && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_sched_core[i];
		}
		for (int i = 0; i < count_pid && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_pid[i];
		}
		for (int i = 0; i < count_rbtree && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_rbtree[i];
		}
		all_cases = merged;
		all_count = idx;
	}
	*out = all_cases;
	return all_count;
}
