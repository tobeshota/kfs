#ifndef KFS_CONSOLE_H
#define KFS_CONSOLE_H

#include <kfs/stddef.h>
#include <kfs/stdint.h>
#include <video/vga.h>

#define KFS_VGA_WIDTH 80			/* 1コンソールあたりの幅 */
#define KFS_VGA_HEIGHT 25			/* 1コンソールあたりの高さ(行数) */
#define KFS_VIRTUAL_CONSOLE_COUNT 4 /* 仮想コンソールの数 */

uint8_t kfs_vga_make_color(enum vga_color fg, enum vga_color bg);
uint16_t kfs_vga_make_entry(char c, uint8_t color);

/* 初期化ルーチン (従来 umbrella 経由で公開) */
void terminal_initialize(void);

/* 端末出力 API */
void terminal_putchar(char c);
void terminal_putchar_overwrite(char c);
void terminal_delete_char(void);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *s);

void kfs_terminal_move_cursor(size_t row, size_t column);
void kfs_terminal_get_cursor(size_t *row, size_t *column);
void kfs_terminal_set_color(uint8_t color);
uint8_t kfs_terminal_get_color(void);

size_t kfs_terminal_active_console(void);
size_t kfs_terminal_console_count(void);
void kfs_terminal_switch_console(size_t index);

/* スクロール機能 */
void kfs_terminal_scroll_up(void);
void kfs_terminal_scroll_down(void);

/* カーソル移動（左右の矢印キー用） */
void kfs_terminal_cursor_left(void);
void kfs_terminal_cursor_right(void);

#endif /* KFS_CONSOLE_H */
