#include <linux/string.h> /* strlen */
#include <linux/terminal.h>
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

/* Internal terminal state (kept global; not part of public header). */
size_t kfs_terminal_row;
size_t kfs_terminal_column;
uint8_t kfs_terminal_color;
uint16_t *kfs_terminal_buffer = (uint16_t *)VGA_MEMORY; /* Default VGA memory */

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
			kfs_terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', kfs_terminal_color);
		}
	}
}

void terminal_initialize(void)
{
	kfs_terminal_row = 0;
	kfs_terminal_column = 0;
	kfs_terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_clear();
}

void terminal_setcolor(uint8_t color)
{
	kfs_terminal_color = color;
}

static void terminal_scroll_if_needed(void)
{
	if (kfs_terminal_row < VGA_HEIGHT)
		return;
	for (size_t y = 1; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
			kfs_terminal_buffer[(y - 1) * VGA_WIDTH + x] = kfs_terminal_buffer[y * VGA_WIDTH + x];
	}
	for (size_t x = 0; x < VGA_WIDTH; x++)
		kfs_terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', kfs_terminal_color);
	kfs_terminal_row = VGA_HEIGHT - 1;
}

static void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	kfs_terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

void terminal_putchar(char c)
{
	if (c == '\n')
	{
		kfs_terminal_column = 0;
		kfs_terminal_row++;
		terminal_scroll_if_needed();
		return;
	}
	if (c == '\r')
	{
		kfs_terminal_column = 0;
		return;
	}
	terminal_putentryat(c, kfs_terminal_color, kfs_terminal_column, kfs_terminal_row);
	if (++kfs_terminal_column == VGA_WIDTH)
	{
		kfs_terminal_column = 0;
		kfs_terminal_row++;
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
	// Required to capture VGA output during integration testing. Remove once the mechanism is in place.
	serial_write(s, strlen(s));
	terminal_write(s, strlen(s));
}
