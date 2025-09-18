#include <linux/string.h> /* strlen */
#include <linux/terminal.h>
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer = (uint16_t *)VGA_MEMORY; /* Default VGA memory; tests may override */

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg)
{
	return fg | bg << 4;
}
static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
	return (uint16_t)uc | (uint16_t)color << 8;
}

static void terminal_clear(void)
{
	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_initialize(void)
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_clear();
}

void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}

static void terminal_scroll_if_needed(void)
{
	if (terminal_row < VGA_HEIGHT)
		return;
	for (size_t y = 1; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			terminal_buffer[(y - 1) * VGA_WIDTH + x] = terminal_buffer[y * VGA_WIDTH + x];
		}
	}
	for (size_t x = 0; x < VGA_WIDTH; x++)
		terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
	terminal_row = VGA_HEIGHT - 1;
}

static void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

void terminal_putchar(char c)
{
	if (c == '\n')
	{
		terminal_column = 0;
		terminal_row++;
		terminal_scroll_if_needed();
		return;
	}
	if (c == '\r')
	{
		terminal_column = 0;
		return;
	}
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH)
	{
		terminal_column = 0;
		terminal_row++;
		terminal_scroll_if_needed();
	}
}

void terminal_write(const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}
void terminal_writestring(const char *s)
{
	serial_write(s, strlen(s)); //Required to capture VGA output during integration testing. Remove once the mechanism is in place.
	terminal_write(s, strlen(s));
}

/* --- Test/host support hooks (not declared in public header) ---
 * These allow unit tests to inject a stub buffer so that production
 * terminal logic can be executed without accessing real VGA memory.
 * Kernel code does not call these functions; they are optional.
 */
void kfs_terminal_set_buffer(uint16_t *buf)
{
	if (buf)
		terminal_buffer = buf;
	else
		terminal_buffer = (uint16_t *)VGA_MEMORY;
}

uint16_t *kfs_terminal_get_buffer(void)
{
	return terminal_buffer;
}

size_t kfs_terminal_get_row(void)
{
	return terminal_row;
}
size_t kfs_terminal_get_col(void)
{
	return terminal_column;
}
uint8_t kfs_terminal_get_color(void)
{
	return terminal_color;
}
