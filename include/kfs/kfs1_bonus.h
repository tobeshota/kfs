#ifndef KFS_KFS1_BONUS_H
#define KFS_KFS1_BONUS_H

#include <linux/terminal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define KFS_KERN_SOH "\001"
#define KERN_SOH KFS_KERN_SOH
#define KERN_EMERG KFS_KERN_SOH "0"
#define KERN_ALERT KFS_KERN_SOH "1"
#define KERN_CRIT KFS_KERN_SOH "2"
#define KERN_ERR KFS_KERN_SOH "3"
#define KERN_WARNING KFS_KERN_SOH "4"
#define KERN_NOTICE KFS_KERN_SOH "5"
#define KERN_INFO KFS_KERN_SOH "6"
#define KERN_DEBUG KFS_KERN_SOH "7"
#define KERN_DEFAULT KFS_KERN_SOH "d"
#define KERN_CONT KFS_KERN_SOH "c"

#define KFS_VGA_WIDTH 80
#define KFS_VGA_HEIGHT 25
#define KFS_VIRTUAL_CONSOLE_COUNT 4

	enum kfs_printk_loglevel
	{
		KFS_LOGLEVEL_EMERG = 0,
		KFS_LOGLEVEL_ALERT = 1,
		KFS_LOGLEVEL_CRIT = 2,
		KFS_LOGLEVEL_ERR = 3,
		KFS_LOGLEVEL_WARNING = 4,
		KFS_LOGLEVEL_NOTICE = 5,
		KFS_LOGLEVEL_INFO = 6,
		KFS_LOGLEVEL_DEBUG = 7,
		KFS_LOGLEVEL_CONT = 8,
		KFS_LOGLEVEL_DEFAULT = KFS_LOGLEVEL_WARNING,
	};

	uint8_t kfs_vga_make_color(enum vga_color fg, enum vga_color bg);
	uint16_t kfs_vga_make_entry(char c, uint8_t color);

	void kfs_terminal_move_cursor(size_t row, size_t column);
	void kfs_terminal_get_cursor(size_t *row, size_t *column);
	void kfs_terminal_set_color(uint8_t color);
	uint8_t kfs_terminal_get_color(void);

	size_t kfs_terminal_active_console(void);
	size_t kfs_terminal_console_count(void);
	void kfs_terminal_switch_console(size_t index);

	void kfs_keyboard_init(void);
	void kfs_keyboard_reset(void);
	void kfs_keyboard_feed_scancode(uint8_t scancode);
	void kfs_keyboard_poll(void);

	void kfs_printk_set_console_loglevel(int level);
	int kfs_printk_get_console_loglevel(void);
	int kfs_printk_get_default_loglevel(void);
	int kfs_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
	int kfs_snprintf(char *buf, size_t size, const char *fmt, ...);

	int printk(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* KFS_KFS1_BONUS_H */
