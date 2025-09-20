#include <linux/console.h>
#include <linux/printk.h>
#include <linux/serial.h>

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/vga.h>
#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH KFS_VGA_WIDTH
#define VGA_HEIGHT KFS_VGA_HEIGHT
#define VGA_MEMORY 0xB8000
#define VGA_CRTC_COMMAND_PORT 0x3D4
#define VGA_CRTC_DATA_PORT 0x3D5

extern void kfs_io_outb(uint16_t port, uint8_t val);

size_t kfs_terminal_row;
size_t kfs_terminal_column;
uint8_t kfs_terminal_color;
uint16_t *kfs_terminal_buffer = (uint16_t *)VGA_MEMORY;

struct kfs_console_state
{
	size_t row;
	size_t column;
	uint8_t color;
	uint16_t shadow[VGA_WIDTH * VGA_HEIGHT];
	int initialized;
};

static struct kfs_console_state kfs_console_states[KFS_VIRTUAL_CONSOLE_COUNT];
static size_t kfs_console_active;
static int kfs_console_bootstrap_completed;

static struct kfs_console_state *active_console(void)
{
	return &kfs_console_states[kfs_console_active];
}

static int console_is_active(const struct kfs_console_state *con)
{
	return con == &kfs_console_states[kfs_console_active];
}

uint8_t kfs_vga_make_color(enum vga_color fg, enum vga_color bg)
{
	return (uint8_t)(fg | (bg << 4));
}

uint16_t kfs_vga_make_entry(char c, uint8_t color)
{
	return (uint16_t)c | ((uint16_t)color << 8);
}

static void console_fill_blank(struct kfs_console_state *con)
{
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
		con->shadow[i] = blank;
}

static void console_flush_to_hw(const struct kfs_console_state *con)
{
	if (!kfs_terminal_buffer)
		return;
	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
		kfs_terminal_buffer[i] = con->shadow[i];
}

static void console_capture_from_hw(struct kfs_console_state *con)
{
	if (!kfs_terminal_buffer)
		return;
	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
		con->shadow[i] = kfs_terminal_buffer[i];
}

static void ensure_console_bootstrap(void)
{
	if (kfs_console_bootstrap_completed)
		return;
	uint8_t default_color = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	for (size_t i = 0; i < KFS_VIRTUAL_CONSOLE_COUNT; ++i)
	{
		struct kfs_console_state *con = &kfs_console_states[i];
		con->row = 0;
		con->column = 0;
		con->color = default_color;
		con->initialized = 0;
		console_fill_blank(con);
	}
	kfs_console_active = 0;
	kfs_console_bootstrap_completed = 1;
}

static void terminal_update_hw_cursor(size_t row, size_t column)
{
	uint16_t pos = (uint16_t)(row * VGA_WIDTH + column);
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, 0x0F);
	kfs_io_outb(VGA_CRTC_DATA_PORT, (uint8_t)(pos & 0xFF));
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, 0x0E);
	kfs_io_outb(VGA_CRTC_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));
}

static void sync_globals_from_console(const struct kfs_console_state *con)
{
	kfs_terminal_row = con->row;
	kfs_terminal_column = con->column;
	kfs_terminal_color = con->color;
	terminal_update_hw_cursor(kfs_terminal_row, kfs_terminal_column);
}

static void console_activate_if_needed(struct kfs_console_state *con)
{
	if (!con->initialized)
	{
		con->row = 0;
		con->column = 0;
		con->color = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
		con->initialized = 1;
		console_fill_blank(con);
		if (console_is_active(con))
			console_flush_to_hw(con);
	}
}

void terminal_initialize(void)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	con->color = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	con->row = 0;
	con->column = 0;
	con->initialized = 1;
	console_fill_blank(con);
	console_flush_to_hw(con);
	sync_globals_from_console(con);
}

void terminal_setcolor(uint8_t color)
{
	kfs_terminal_set_color(color);
}

static void terminal_putentryat(struct kfs_console_state *con, char c, size_t x, size_t y)
{
	uint16_t entry = kfs_vga_make_entry(c, con->color);
	con->shadow[y * VGA_WIDTH + x] = entry;
	if (console_is_active(con) && kfs_terminal_buffer)
		kfs_terminal_buffer[y * VGA_WIDTH + x] = entry;
}

static void terminal_scroll_if_needed(struct kfs_console_state *con)
{
	if (con->row < VGA_HEIGHT)
		return;
	int flush_hw = console_is_active(con) && kfs_terminal_buffer;
	for (size_t y = 1; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			uint16_t value = con->shadow[y * VGA_WIDTH + x];
			con->shadow[(y - 1) * VGA_WIDTH + x] = value;
			if (flush_hw)
				kfs_terminal_buffer[(y - 1) * VGA_WIDTH + x] = value;
		}
	}
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	for (size_t x = 0; x < VGA_WIDTH; x++)
	{
		con->shadow[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
		if (flush_hw)
			kfs_terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
	}
	con->row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	console_activate_if_needed(con);
	if (c == '\n')
	{
		con->column = 0;
		con->row++;
		terminal_scroll_if_needed(con);
		sync_globals_from_console(con);
		return;
	}
	if (c == '\r')
	{
		con->column = 0;
		sync_globals_from_console(con);
		return;
	}
	terminal_putentryat(con, c, con->column, con->row);
	if (++con->column == VGA_WIDTH)
	{
		con->column = 0;
		con->row++;
	}
	terminal_scroll_if_needed(con);
	sync_globals_from_console(con);
}

void terminal_write(const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char *s)
{
	serial_write(s, strlen(s));
	terminal_write(s, strlen(s));
}

void kfs_terminal_move_cursor(size_t row, size_t column)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	if (row >= VGA_HEIGHT)
		row = VGA_HEIGHT - 1;
	if (column >= VGA_WIDTH)
		column = VGA_WIDTH - 1;
	con->row = row;
	con->column = column;
	sync_globals_from_console(con);
}

void kfs_terminal_get_cursor(size_t *row, size_t *column)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	if (row)
		*row = con->row;
	if (column)
		*column = con->column;
}

void kfs_terminal_set_color(uint8_t color)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	con->color = color;
	kfs_terminal_color = color;
}

uint8_t kfs_terminal_get_color(void)
{
	ensure_console_bootstrap();
	return active_console()->color;
}

size_t kfs_terminal_active_console(void)
{
	ensure_console_bootstrap();
	return kfs_console_active;
}

size_t kfs_terminal_console_count(void)
{
	return KFS_VIRTUAL_CONSOLE_COUNT;
}

void kfs_terminal_switch_console(size_t index)
{
	ensure_console_bootstrap();
	if (index >= KFS_VIRTUAL_CONSOLE_COUNT || index == kfs_console_active)
		return;
	struct kfs_console_state *current = active_console();
	if (current->initialized)
		console_capture_from_hw(current);
	kfs_console_active = index;
	struct kfs_console_state *next = active_console();
	console_activate_if_needed(next);
	console_flush_to_hw(next);
	sync_globals_from_console(next);
}
