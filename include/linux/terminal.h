/* Terminal API (transitional)
 * TODO: 分割予定 -> console(core state) / tty(input discipline) / video(flush) / serial bridging
 */
#ifndef LINUX_TERMINAL_H
#define LINUX_TERMINAL_H

#include <linux/vga.h>
#include <stddef.h>
#include <stdint.h>

/* NOTE: VGA color enum moved to <linux/vga.h>. */

#ifdef __cplusplus
extern "C"
{
#endif

	/* Serial */
	void serial_init(void);
	void serial_write(const char *data, size_t size);

	/* Terminal */
	void terminal_initialize(void);
	void terminal_setcolor(uint8_t color);
	void terminal_putchar(char c);
	void terminal_write(const char *data, size_t size);
	void terminal_writestring(const char *data);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_TERMINAL_H */
