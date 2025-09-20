#include "host_test_framework.h"

// 各テストファイルで提供される register_* 関数
int register_host_tests_init_main(struct kfs_test_case **out);
int register_host_tests_terminal_edge(struct kfs_test_case **out);
int register_host_tests_terminal_prod(struct kfs_test_case **out);
int register_host_tests_terminal_bonus(struct kfs_test_case **out);
int register_host_tests_serial(struct kfs_test_case **out);
int register_host_tests_kernel_main(struct kfs_test_case **out);
int register_host_tests_printk(struct kfs_test_case **out);
int register_host_tests_keyboard(struct kfs_test_case **out);
int register_host_tests_string(struct kfs_test_case **out);

#define KFS_MAX_TESTS 256

// すべてのテストケースを一つにまとめる
static struct kfs_test_case *all_cases = 0;
static int all_count = 0;

int register_host_tests(struct kfs_test_case **out)
{
	// 初回だけ収集
	if (!all_cases)
	{
		struct kfs_test_case *cases_init = 0;
		int count_init = register_host_tests_init_main(&cases_init);
		struct kfs_test_case *cases_term_edge = 0;
		int count_term_edge = register_host_tests_terminal_edge(&cases_term_edge);
		struct kfs_test_case *cases_term_prod = 0;
		int count_term_prod = register_host_tests_terminal_prod(&cases_term_prod);
		struct kfs_test_case *cases_term_bonus = 0;
		int count_term_bonus = register_host_tests_terminal_bonus(&cases_term_bonus);
		struct kfs_test_case *cases_serial = 0;
		int count_serial = register_host_tests_serial(&cases_serial);
		struct kfs_test_case *cases_kernel = 0;
		int count_kernel = register_host_tests_kernel_main(&cases_kernel);
		struct kfs_test_case *cases_printk = 0;
		int count_printk = register_host_tests_printk(&cases_printk);
		struct kfs_test_case *cases_keyboard = 0;
		int count_keyboard = register_host_tests_keyboard(&cases_keyboard);
		struct kfs_test_case *cases_string = 0;
		int count_string = register_host_tests_string(&cases_string);
		// 動的確保は避け、静的最大数 (今は少数) を想定してスタック上に置けないので静的配列
		static struct kfs_test_case merged[KFS_MAX_TESTS];
		int idx = 0;
		for (int i = 0; i < count_init && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_init[i];
		for (int i = 0; i < count_term_edge && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_term_edge[i];
		for (int i = 0; i < count_term_prod && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_term_prod[i];
		for (int i = 0; i < count_term_bonus && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_term_bonus[i];
		for (int i = 0; i < count_serial && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_serial[i];
		for (int i = 0; i < count_kernel && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_kernel[i];
		for (int i = 0; i < count_printk && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_printk[i];
		for (int i = 0; i < count_keyboard && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_keyboard[i];
		for (int i = 0; i < count_string && idx < KFS_MAX_TESTS; i++)
			merged[idx++] = cases_string[i];
		all_cases = merged;
		all_count = idx;
	}
	*out = all_cases;
	return all_count;
}
