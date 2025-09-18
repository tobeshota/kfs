/* Minimal kernel entry using new driver layout */
#include <linux/terminal.h>

void kernel_main(void)
{
	serial_init();
	terminal_initialize();
	/* Integration test expects '42' on the serial (COM1) output. */
	serial_write("42\n", 3);
	terminal_writestring("42\n");
	terminal_writestring("Hello, kernel World!\n");
	terminal_writestring("This is a minimal i386 kernel.\n");
	terminal_writestring("Newline and scrolling are supported.\n");
}

/* テスト用ラッパは drivers 実装側に残存 */
