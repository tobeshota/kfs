#include <asm-i386/desc.h>
#include <asm-i386/i8259.h>
#include <asm-i386/page.h>
#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/pcspkr.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/pty.h>
#include <kfs/sched.h>
#include <kfs/serial.h>
#include <kfs/shell.h>
#include <kfs/slab.h>
#include <kfs/timer.h>
#include <kfs/vmalloc.h>
#include <kfs/wait.h>

/** Multiboot情報構造体へのポインタ（boot.Sで設定）
 * @note このポインタ自体は.boot.dataセクション（物理アドレス）にあり，
 *       その値（multiboot_infoのアドレス）も物理アドレスである．
 *       使用時に__va()で仮想アドレスに変換する必要がある．
 */
extern unsigned long multiboot_info_ptr;
extern uint32_t multiboot_magic; /* boot.S で保存したブートローダーマジック */

/* ページアロケータの初期化（mm/page_alloc.c） */
extern void page_alloc_init(unsigned long mbi_ptr, uint32_t magic);

/** PID 1: init プロセス
 * shell を子プロセス（PID 2）として起動し、
 * 孤児プロセス（音楽バックグラウンド再生等）を wait() で回収し続ける。
 * Linux の PID 1 / init に相当する。
 */
static void kernel_init(void)
{
	/* 各仮想コンソールごとにシェルを1つ起動する */
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
	for (size_t i = 0; i < kfs_terminal_console_count(); ++i)
	{
		pid_t pid = do_fork((unsigned long)shell_run);
		if (pid < 0)
		{
			printk("Failed to start shell process on console %d\n", (int)i);
			continue;
		}

		struct task_struct *shell_task = find_task_by_pid(pid);
		if (!shell_task)
		{
			continue;
		}

		shell_task->tty_console = i;
		shell_task->pgrp = pid;
		shell_task->session = pid;
		(void)kfs_terminal_set_foreground_pgrp_for_console(i, pid);
	}

	/* 孤児プロセス（バックグラウンド再生等）を回収するループ
	 * do_wait() はEXIT_ZOMBIEの孤児が現れるまでブロックし、回収後にまたブロックする。 */
	while (1)
	{
		do_wait(NULL, 0);
	}
}

void start_kernel(void)
{
	serial_init();
	pcspkr_init();
	psg_init();
	terminal_initialize();
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

	/* Integration test expects '42' on the serial (COM1) output. */
	printk("42\n");

	/* Debug: show segment selectors after gdt_init */
	unsigned short cs, ds, ss;
	asm volatile("mov %%cs, %0" : "=r"(cs)); /* 現在のCSレジスタ %%cs を C変数 cs に書き込む */
	asm volatile("mov %%ds, %0" : "=r"(ds)); /* 現在のDSレジスタ %%ds を C変数 ds に書き込む */
	asm volatile("mov %%ss, %0" : "=r"(ss)); /* 現在のSSレジスタ %%ss を C変数 ss に書き込む */
	printk("GDT loaded: CS=%x DS=%x SS=%x\n", cs, ds, ss);

	/* IDT初期化（割り込み/例外ハンドラを登録） */
	idt_init();

	/* PIC(8259A)初期化（IRQをベクタ0x20-0x2Fにリマップ） */
	printk("Initializing 8259A PIC...\n");
	init_8259A();

	/* スケジューラ初期化（ランキュー確立・init_task 登録） */
	sched_init();

	/* PIT タイマー初期化（IRQ0 → scheduler_tick() を毎 1ms 呼び出し） */
	timer_init();

	/* PTY 初期化 */
	pty_reset();

	/* PS/2キーボードドライバを初期化する */
	kfs_keyboard_init();

	/* メモリ管理システムの初期化 */
	if (multiboot_info_ptr != 0)
	{
		printk("Initializing memory management...\n");
		page_alloc_init(multiboot_info_ptr, multiboot_magic);

		/* Slabアロケータ初期化（kmalloc/kfree使用可能に） */
		kmem_cache_init();

		/* プロセス管理初期化（task_struct_cachep確立） */
		fork_init();
		vmalloc_init();

		mem_init();
	}
	else
	{
		printk("Multiboot info not available\n");
	}

	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	printk("Alt+F1..F4 switch consoles; keyboard echo ready.\n");

	/* PID 1 の init プロセスを起動する（シェルの展開と孤児回収を担当） */
	kernel_thread(kernel_init, "init");

	/* init_task はここから cpu_idle_lo	op() でアイドル待機する。
	 * この呼び出しから戻ることはない。 */
	cpu_idle_loop();
}
