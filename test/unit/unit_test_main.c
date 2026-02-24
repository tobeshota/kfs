#include "coverage/coverage.h"
#include "unit_test_framework.h"
#include <asm-i386/desc.h>
#include <asm-i386/i8259.h>
#include <asm-i386/io.h>
#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/serial.h>
#include <kfs/shell.h>
#include <kfs/timer.h>

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
	extern unsigned long multiboot_info_ptr;
	extern uint32_t multiboot_magic;
	extern void page_alloc_init(unsigned long mbi_ptr, uint32_t magic);
	extern void kmem_cache_init(void);

	if (multiboot_info_ptr != 0)
	{
		page_alloc_init(multiboot_info_ptr, multiboot_magic);
		kmem_cache_init();
	}

	/* IDT初期化（INT 0x80 = ring-3からのsyscall用） */
	idt_init();

	/* スケジューラ初期化（RRキュー確立・init_task登録） */
	extern void init_idle_task(void);
	extern void fork_init(void);
	extern void pid_init(void);
	init_idle_task();
	sched_init();

	/* PIT タイマー初期化（IRQ0 → scheduler_tick()） */
	/* 8259A PIC を先に初期化しないと IRQ0 が vector 0x08（DF）に飛ぶ */
	init_8259A();
	timer_init();

	/* fork・PID初期化（task_struct_cachep確立） */
	fork_init();
	pid_init();

	extern int register_unit_tests(struct kfs_test_case * *out);
	struct kfs_test_case *cases = 0;
	int count = register_unit_tests(&cases);
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
