#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/shell.h>

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

	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	printk("Alt+F1..F4 switch consoles; keyboard echo ready.\n");

	/* シェルを起動（無限ループに入る）
	 * テスト環境ではshell_run()がオーバーライドされてすぐに戻る */
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
	shell_run();
}
