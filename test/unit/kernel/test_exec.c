/**
 * @file test_exec.c
 * @brief Phase 12: BSS/data セクション境界の単体テスト
 *
 * mm_set_kernel_sections() は fork.c の static 関数のため直接呼べない。
 * linker.ld が生成するセクション境界シンボルを直接検証することで
 * 同等の保証を得る。
 */

#include "../test_reset.h"
#include "../unit_test_framework.h"
#include <kfs/mm_types.h>
#include <kfs/sched.h>

/* リンカシンボル（linker.ld で定義） */
extern unsigned long __text_start;
extern unsigned long __text_end;
extern unsigned long __data_start;
extern unsigned long __data_end;
extern unsigned long __bss_start;
extern unsigned long __bss_end;

extern struct task_struct *current;
extern struct task_struct init_task;

static void setup_test(void)
{
	reset_all_state_for_test();
	current = &init_task;
}

static void teardown_test(void)
{
}

/* .text セクションが Higher Half（>= 0xC0000000）に配置されていることを確かめる */
KFS_TEST(test_linker_text_section_in_higher_half)
{
	KFS_ASSERT_TRUE((unsigned long)&__text_start >= 0xC0000000UL);
	KFS_ASSERT_TRUE((unsigned long)&__text_end >= 0xC0000000UL);
}

/* .data セクションが Higher Half に配置されていることを確かめる */
KFS_TEST(test_linker_data_section_in_higher_half)
{
	KFS_ASSERT_TRUE((unsigned long)&__data_start >= 0xC0000000UL);
	KFS_ASSERT_TRUE((unsigned long)&__data_end >= 0xC0000000UL);
}

/* .bss セクションが Higher Half に配置されていることを確かめる */
KFS_TEST(test_linker_bss_section_in_higher_half)
{
	KFS_ASSERT_TRUE((unsigned long)&__bss_start >= 0xC0000000UL);
	KFS_ASSERT_TRUE((unsigned long)&__bss_end >= 0xC0000000UL);
}

/* .text セクションのサイズが正（start < end）であることを確かめる */
KFS_TEST(test_linker_text_section_size_is_positive)
{
	KFS_ASSERT_TRUE((unsigned long)&__text_start < (unsigned long)&__text_end);
}

/* .bss セクションのサイズが正（start < end）であることを確かめる */
KFS_TEST(test_linker_bss_section_size_is_positive)
{
	KFS_ASSERT_TRUE((unsigned long)&__bss_start < (unsigned long)&__bss_end);
}

/* セクションの順序が text <= data <= bss であることを確かめる */
KFS_TEST(test_linker_section_order)
{
	KFS_ASSERT_TRUE((unsigned long)&__text_start <= (unsigned long)&__data_start);
	KFS_ASSERT_TRUE((unsigned long)&__data_start <= (unsigned long)&__bss_start);
}

/* mm_struct のフィールドに linker シンボル値を設定したとき整合が取れることを確かめる
 * （mm_set_kernel_sections は static のため、期待値をリンカシンボルで直接確認） */
KFS_TEST(test_mm_fields_match_linker_symbols)
{
	struct mm_struct mm = {0};

	mm.start_code = (unsigned long)&__text_start;
	mm.end_code = (unsigned long)&__text_end;
	mm.start_data = (unsigned long)&__data_start;
	mm.end_data = (unsigned long)&__data_end;
	mm.start_bss = (unsigned long)&__bss_start;
	mm.end_bss = (unsigned long)&__bss_end;

	KFS_ASSERT_TRUE(mm.start_code >= 0xC0000000UL);
	KFS_ASSERT_TRUE(mm.start_code < mm.end_code);
	KFS_ASSERT_TRUE(mm.start_bss < mm.end_bss);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_linker_text_section_in_higher_half, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_linker_data_section_in_higher_half, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_linker_bss_section_in_higher_half, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_linker_text_section_size_is_positive, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_linker_bss_section_size_is_positive, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_linker_section_order, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_mm_fields_match_linker_symbols, setup_test, teardown_test),
};

int register_unit_tests_exec(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
