#ifndef KFS_TERMINAL_H
#define KFS_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Public VGA color enumeration */
	enum vga_color
	{
		VGA_COLOR_BLACK = 0,
		VGA_COLOR_BLUE = 1,
		VGA_COLOR_GREEN = 2,
		VGA_COLOR_CYAN = 3,
		VGA_COLOR_RED = 4,
		VGA_COLOR_MAGENTA = 5,
		VGA_COLOR_BROWN = 6,
		VGA_COLOR_LIGHT_GREY = 7,
		VGA_COLOR_DARK_GREY = 8,
		VGA_COLOR_LIGHT_BLUE = 9,
		VGA_COLOR_LIGHT_GREEN = 10,
		VGA_COLOR_LIGHT_CYAN = 11,
		VGA_COLOR_LIGHT_RED = 12,
		VGA_COLOR_LIGHT_MAGENTA = 13,
		VGA_COLOR_LIGHT_BROWN = 14,
		VGA_COLOR_WHITE = 15,
	};

	/* Public API */
	void terminal_initialize(void);
	void terminal_setcolor(uint8_t color);
	void terminal_putchar(char c);
	void terminal_write(const char *data, size_t size);
	void terminal_writestring(const char *data);

/* Optionally exported test helpers when building with KFS_UNIT_TEST */
#ifdef KFS_UNIT_TEST
	uint16_t kfs_test_vga_entry(unsigned char uc, uint8_t color);
	uint8_t kfs_test_vga_entry_color(enum vga_color fg, enum vga_color bg);
	size_t kfs_test_strlen(const char *s);
	void kfs_test_term_init(void);
	void kfs_test_term_putc(char c);
	void kfs_test_term_write(const char *s);
	size_t kfs_term_get_row(void);
	size_t kfs_term_get_col(void);
	uint8_t kfs_term_get_color(void);
	uint16_t kfs_term_peek(size_t x, size_t y);
#endif

#ifdef __cplusplus
}
#endif

#endif /* KFS_TERMINAL_H */
