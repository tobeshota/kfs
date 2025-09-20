#ifndef LINUX_CONSOLE_H
#define LINUX_CONSOLE_H

/* console.h: VGA テキストモード端末/仮想コンソール関連の公開インタフェース。
 * 既存実装(drivers/video/terminal.c)から宣言のみ抽出。機能追加なし。
 */

#include <video/vga.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define KFS_VGA_WIDTH 80
#define KFS_VGA_HEIGHT 25
#define KFS_VIRTUAL_CONSOLE_COUNT 4

	uint8_t kfs_vga_make_color(enum vga_color fg, enum vga_color bg);
	uint16_t kfs_vga_make_entry(char c, uint8_t color);

	/* 初期化ルーチン (従来 umbrella 経由で公開) */
	void terminal_initialize(void);

	/* 端末出力 API */
	void terminal_putchar(char c);
	void terminal_write(const char *data, size_t size);
	void terminal_writestring(const char *s);

	void kfs_terminal_move_cursor(size_t row, size_t column);
	void kfs_terminal_get_cursor(size_t *row, size_t *column);
	void kfs_terminal_set_color(uint8_t color);
	uint8_t kfs_terminal_get_color(void);

	size_t kfs_terminal_active_console(void);
	size_t kfs_terminal_console_count(void);
	void kfs_terminal_switch_console(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_CONSOLE_H */
