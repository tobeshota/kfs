/* Minimal kernel entry using new driver layout */
#include <kfs/kfs1_bonus.h>

void kernel_main(void)
{
	serial_init();
	terminal_initialize();
	kfs_keyboard_init();
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
	/* Integration test expects '42' on the serial (COM1) output. */
	printk("42\n");
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	printk("Alt+F1..F4 switch consoles; keyboard echo ready.\n");
	kfs_terminal_set_color(kfs_vga_make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
/*
シャットダウンの代替として、キーボードのポーリングをループさせない。
以下はシャットダウンを実装した場合、WAITING_FOR_INPUTによる処理の分岐を削除する。
*/
#ifndef WAITING_FOR_INPUT
	for (;;)
		kfs_keyboard_poll();
#else
	kfs_keyboard_poll();
#endif
}

/* テスト用ラッパは drivers 実装側に残存 */
