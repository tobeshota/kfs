/* Minimal kernel entry using new driver layout */
#include <linux/terminal.h>

void kernel_main(void)
{
	serial_init();
	terminal_initialize();
	/* Integration test expects '42' on the serial (COM1) output. */
	terminal_writestring("42\n");
}

/* テスト用ラッパは drivers 実装側に残存 */
