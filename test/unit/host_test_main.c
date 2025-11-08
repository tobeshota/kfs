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

	extern int register_host_tests(struct kfs_test_case * *out);
	struct kfs_test_case *cases = 0;
	int count = register_host_tests(&cases);
	int result = kfs_run_all_tests(cases, count);

	/* Dump coverage data before exit */

	/* Signal QEMU to exit with status "result" using isa-debug-exit device. */
	outb(0xF4, (uint8_t)result);

	/* Halt; ensure no further execution. */
	for (;;)
		__asm__ volatile("hlt");
}
