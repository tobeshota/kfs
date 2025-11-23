#include "coverage/simple_coverage.h"
#include "host_test_framework.h"
#include <asm-i386/io.h>
#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/shell.h>

int kfs_test_failures = 0;

void start_unit_test_kernel(void)
{
	/* 初期化 */
	serial_init();
	terminal_initialize();
	kfs_keyboard_init();
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

	printk("unit test\n");

	/* メモリ管理の初期化（slabテスト用） */
	extern struct multiboot_info *multiboot_info_ptr;
	extern void page_alloc_init(struct multiboot_info * mbi);
	extern void paging_init(void);
	extern void kmem_cache_init(void);

	if (multiboot_info_ptr != NULL)
	{
		page_alloc_init(multiboot_info_ptr);
		paging_init();
		kmem_cache_init();
	}

	extern int register_host_tests(struct kfs_test_case * *out);
	struct kfs_test_case *cases = 0;
	int count = register_host_tests(&cases);
	int result = kfs_run_all_tests(cases, count);

	/* Dump coverage data before exit */
	coverage_dump();

	/* Signal QEMU to exit with status "result" using isa-debug-exit device. */
	outb(0xF4, (uint8_t)result);

	/* Halt; ensure no further execution. */
	for (;;)
	{
		__asm__ volatile("hlt");
	}
}
