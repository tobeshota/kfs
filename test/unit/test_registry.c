#include "host_test_framework.h"

// 各テストファイルで提供される register_* 関数
int register_host_tests_terminal_scroll(struct kfs_test_case **out);
int register_host_tests_start_kernel(struct kfs_test_case **out);
int register_host_tests_string(struct kfs_test_case **out);
int register_host_tests_page_alloc(struct kfs_test_case **out);
int register_host_tests_shell(struct kfs_test_case **out);
int register_host_tests_keyboard(struct kfs_test_case **out);
int register_host_tests_printk(struct kfs_test_case **out);
int register_host_tests_reboot(struct kfs_test_case **out);
int register_host_tests_stacktrace(struct kfs_test_case **out);
int register_host_tests_memory(struct kfs_test_case **out);
int register_host_tests_slab(struct kfs_test_case **out);
int register_host_tests_vmalloc(struct kfs_test_case **out);

#define KFS_MAX_TESTS 256

// すべてのテストケースを一つにまとめる
static struct kfs_test_case *all_cases = 0;
static int all_count = 0;

int register_host_tests(struct kfs_test_case **out)
{
	// 初回だけ収集
	if (!all_cases)
	{
		struct kfs_test_case *cases_term_scroll = 0;
		int count_term_scroll = register_host_tests_terminal_scroll(&cases_term_scroll);
		// struct kfs_test_case *cases_serial = 0;
		// int count_serial = register_host_tests_serial(&cases_serial);
		struct kfs_test_case *cases_kernel = 0;
		int count_kernel = register_host_tests_start_kernel(&cases_kernel);
		struct kfs_test_case *cases_printk = 0;
		int count_printk = register_host_tests_printk(&cases_printk);
		struct kfs_test_case *cases_reboot = 0;
		int count_reboot = register_host_tests_reboot(&cases_reboot);
		struct kfs_test_case *cases_keyboard = 0;
		int count_keyboard = register_host_tests_keyboard(&cases_keyboard);
		struct kfs_test_case *cases_string = 0;
		int count_string = register_host_tests_string(&cases_string);
		struct kfs_test_case *cases_shell = 0;
		int count_shell = register_host_tests_shell(&cases_shell);
		struct kfs_test_case *cases_page_alloc = 0;
		int count_page_alloc = register_host_tests_page_alloc(&cases_page_alloc);
		struct kfs_test_case *cases_stacktrace = 0;
		int count_stacktrace = register_host_tests_stacktrace(&cases_stacktrace);
		struct kfs_test_case *cases_memory = 0;
		int count_memory = register_host_tests_memory(&cases_memory);
		struct kfs_test_case *cases_slab = 0;
		int count_slab = register_host_tests_slab(&cases_slab);
		struct kfs_test_case *cases_vmalloc = 0;
		int count_vmalloc = register_host_tests_vmalloc(&cases_vmalloc);
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
		for (int i = 0; i < count_reboot && idx < KFS_MAX_TESTS; i++)
		{
			merged[idx++] = cases_reboot[i];
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
		all_cases = merged;
		all_count = idx;
	}
	*out = all_cases;
	return all_count;
}
