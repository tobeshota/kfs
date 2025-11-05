#include "host_test_framework.h"
#include <asm-i386/page.h>
#include <kfs/gfp.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/string.h>

extern unsigned long __alloc_pages(unsigned int gfp_mask);
extern void __free_pages(unsigned long addr);
extern void show_mem_info(void);
extern void mem_init(void);

KFS_TEST(test___alloc_pages)
{
	__alloc_pages(GFP_KERNEL);
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test___free_pages)
{
	__free_pages(0x42);
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_show_mem_info)
{
	show_mem_info();
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_mem_init)
{
	mem_init();
	KFS_ASSERT_TRUE(1);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test___alloc_pages), KFS_REGISTER_TEST(test___free_pages),
	KFS_REGISTER_TEST(test_show_mem_info), KFS_REGISTER_TEST(test_mem_init),
};

int register_host_tests_page_alloc(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
