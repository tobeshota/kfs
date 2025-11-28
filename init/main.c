#include <asm-i386/desc.h>
#include <asm-i386/i8259.h>
#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/mm.h>
#include <kfs/multiboot.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/shell.h>
#include <kfs/slab.h>
#include <kfs/vmalloc.h>

/* Multiboot情報構造体へのポインタ（boot.Sで設定） */
extern struct multiboot_info *multiboot_info_ptr;

/* ページアロケータの初期化（mm/page_alloc.c） */
extern void page_alloc_init(struct multiboot_info *mbi);

void start_kernel(void)
{
	serial_init();
	terminal_initialize();
	kfs_keyboard_init();
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

	/* メモリ管理システムの初期化 */
	if (multiboot_info_ptr != NULL)
	{
		printk("Initializing memory management...\n");
		page_alloc_init(multiboot_info_ptr);

		/* ページング初期化 */
		paging_init();

		/* Slabアロケータ初期化（kmalloc/kfree使用可能に） */
		kmem_cache_init();

		/* 仮想メモリアロケータ初期化（vmalloc/vfree使用可能に） */
		vmalloc_init();

		mem_init();
	}
	else
	{
		printk("Multiboot info not available\n");
	}

	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	printk("Alt+F1..F4 switch consoles; keyboard echo ready.\n");

	/* シェルを起動（無限ループに入る）
	 * テスト環境ではshell_run()がオーバーライドされてすぐに戻る */
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
	shell_run();
}
