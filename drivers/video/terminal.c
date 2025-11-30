#include <kfs/console.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/stddef.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <video/vga.h>

#define VGA_WIDTH KFS_VGA_WIDTH
#define VGA_HEIGHT KFS_VGA_HEIGHT
#define VGA_MEMORY 0xB8000
#define VGA_CRTC_COMMAND_PORT 0x3D4
#define VGA_CRTC_DATA_PORT 0x3D5
#define SCROLLBACK_LINES 100 /* スクロールバックバッファの行数 */

extern void kfs_io_outb(uint16_t port, uint8_t val);

size_t kfs_terminal_row;
size_t kfs_terminal_column;
uint8_t kfs_terminal_color;
uint16_t *kfs_terminal_buffer = (uint16_t *)VGA_MEMORY; /* 画面に文字を書き込むアドレスのラッパ */

struct kfs_console_state
{
	size_t row;
	size_t column;
	uint8_t color;
	uint16_t shadow[VGA_WIDTH * VGA_HEIGHT];
	uint16_t scrollback[SCROLLBACK_LINES * VGA_WIDTH]; /* スクロールバックバッファ */
	size_t scrollback_pos;	 /* スクロールバックバッファ内の現在位置（リングバッファ） */
	size_t scrollback_lines; /* 保存されているスクロールバック行数 */
	int scroll_offset;		 /* 現在のスクロールオフセット（0=最新、正の値=過去） */
	int initialized;
};

static struct kfs_console_state kfs_console_states[KFS_VIRTUAL_CONSOLE_COUNT];
static size_t kfs_console_active;
static int kfs_console_bootstrap_completed;

/* 現在使用してるコンソールを取得 */
static struct kfs_console_state *active_console(void)
{
	return &kfs_console_states[kfs_console_active];
}

static int console_is_active(const struct kfs_console_state *con)
{
	return con == &kfs_console_states[kfs_console_active];
}

/* VGAハードウェアカーソルを更新 */
static void update_hardware_cursor(void)
{
	uint16_t pos = kfs_terminal_row * VGA_WIDTH + kfs_terminal_column;
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, 0x0F);
	kfs_io_outb(VGA_CRTC_DATA_PORT, (uint8_t)(pos & 0xFF));
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, 0x0E);
	kfs_io_outb(VGA_CRTC_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));
}

/* VGAに書き込む文字の前背色と後背色を定義する */
uint8_t kfs_vga_make_color(enum vga_color fg, enum vga_color bg)
{
	return (uint8_t)(fg | (bg << 4));
}

/* 文字と色属性を組み合わせてVGAに書き込むエントリを作成する */
uint16_t kfs_vga_make_entry(char c, uint8_t color)
{
	return (uint16_t)c | ((uint16_t)color << 8);
}

/* シャドウバッファを初期化する(console_flush_to_hw()でコンソールを' '文字、背景黒で埋めるため) */
static void console_fill_blank(struct kfs_console_state *con)
{
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
	{
		con->shadow[i] = blank;
	}
}

/* シャドウバッファをVGAに書き込む */
static void console_flush_to_hw(const struct kfs_console_state *con)
{
	if (!kfs_terminal_buffer)
	{
		return;
	}
	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
	{
		kfs_terminal_buffer[i] = con->shadow[i];
	}
}

/* VGAに書き込まれた値をシャドウバッファに書き込む */
static void console_capture_from_hw(struct kfs_console_state *con)
{
	if (!kfs_terminal_buffer)
	{
		return;
	}
	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
	{
		con->shadow[i] = kfs_terminal_buffer[i];
	}
}

/* 全コンソールを' '文字、背景黒で埋める */
static void ensure_console_bootstrap(void)
{
	if (kfs_console_bootstrap_completed)
	{
		return;
	}
	uint8_t default_color = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	for (size_t i = 0; i < KFS_VIRTUAL_CONSOLE_COUNT; ++i)
	{
		struct kfs_console_state *con = &kfs_console_states[i];
		con->row = 0;
		con->column = 0;
		con->color = default_color;
		con->scrollback_pos = 0;
		con->scrollback_lines = 0;
		con->scroll_offset = 0;
		con->initialized = 0;
		console_fill_blank(con); /* ' '文字、背景黒で埋める */
		/* スクロールバックバッファも空白で初期化 */
		uint16_t blank = kfs_vga_make_entry(' ', default_color);
		for (size_t j = 0; j < SCROLLBACK_LINES * VGA_WIDTH; ++j)
		{
			con->scrollback[j] = blank;
		}
	}
	kfs_console_active = 0;
	kfs_console_bootstrap_completed = 1;
}

/* VGAに出力するカーソルの位置をrow行目、column列目に設定する */
static void terminal_update_hw_cursor(size_t row, size_t column)
{
	uint16_t pos = (uint16_t)(row * VGA_WIDTH + column);
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, 0x0F);
	kfs_io_outb(VGA_CRTC_DATA_PORT, (uint8_t)(pos & 0xFF));
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, 0x0E);
	kfs_io_outb(VGA_CRTC_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));
}

/* VGAに出力するカーソルの位置を書き込まれた文字の最後の位置に表示するようにする */
static void sync_globals_from_console(const struct kfs_console_state *con)
{
	kfs_terminal_row = con->row;
	kfs_terminal_column = con->column;
	kfs_terminal_color = con->color;
	terminal_update_hw_cursor(kfs_terminal_row, kfs_terminal_column);
}

/* 非アクティブなコンソールをアクティブする */
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
		{
			console_flush_to_hw(con);
		}
	}
}

/* コンソールの初期化 */
void terminal_initialize(void)
{
	ensure_console_bootstrap();

	/* 現在使用してるコンソールを取得 */
	struct kfs_console_state *con = active_console();
	con->color = kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	con->row = 0;	 /* 0行目(直感的には画面最上部)から記述を開始する */
	con->column = 0; /* 0列目(直感的には画面最左部)から記述を開始する */
	con->initialized = 1;
	con->scrollback_pos = 0; /* スクロールバックバッファもリセット */
	con->scrollback_lines = 0;
	con->scroll_offset = 0;
	console_fill_blank(con);
	/* スクロールバックバッファも空白で埋める */
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	for (size_t j = 0; j < SCROLLBACK_LINES * VGA_WIDTH; ++j)
	{
		con->scrollback[j] = blank;
	}
	console_flush_to_hw(con);
	sync_globals_from_console(con);
}

void terminal_setcolor(uint8_t color)
{
	kfs_terminal_set_color(color);
}

/* 文字cをコンソールconのVGAの位置(x, y)に出力する（上書きモード） */
static void terminal_putentryat(struct kfs_console_state *con, char c, size_t x, size_t y)
{
	uint16_t entry = kfs_vga_make_entry(c, con->color);
	con->shadow[y * VGA_WIDTH + x] = entry;
	if (console_is_active(con) && kfs_terminal_buffer)
	{
		kfs_terminal_buffer[y * VGA_WIDTH + x] = entry;
	}
}

/* カーソル位置に文字を挿入し、右側の文字列をシフトする（挿入モード） */
static void terminal_insert_char_at(struct kfs_console_state *con, char c, size_t x, size_t y)
{
	/* 現在の行の末尾の文字を保存 */
	uint16_t last_char = con->shadow[y * VGA_WIDTH + (VGA_WIDTH - 1)];

	/* カーソル位置から行末まで1文字ずつ右にシフト */
	for (size_t i = VGA_WIDTH - 1; i > x; i--)
	{
		con->shadow[y * VGA_WIDTH + i] = con->shadow[y * VGA_WIDTH + i - 1];
		if (console_is_active(con) && kfs_terminal_buffer)
		{
			kfs_terminal_buffer[y * VGA_WIDTH + i] = con->shadow[y * VGA_WIDTH + i];
		}
	}

	/* カーソル位置に新しい文字を挿入 */
	uint16_t entry = kfs_vga_make_entry(c, con->color);
	con->shadow[y * VGA_WIDTH + x] = entry;
	if (console_is_active(con) && kfs_terminal_buffer)
	{
		kfs_terminal_buffer[y * VGA_WIDTH + x] = entry;
	}

	/* 行末を超えた文字を次の行の先頭に移動（空白文字でない場合のみ） */
	char last_char_value = (char)(last_char & 0xFF);
	if (last_char_value != ' ' && y + 1 < VGA_HEIGHT)
	{
		/* 次の行にも挿入モードで文字を追加 */
		terminal_insert_char_at(con, last_char_value, 0, y + 1);
	}
}

/* 必要に応じてスクロールする */
static void terminal_scroll_if_needed(struct kfs_console_state *con)
{
	if (con->row < VGA_HEIGHT)
	{
		return;
	}

	/* スクロールアウトする最初の行をスクロールバックバッファに保存 */
	size_t save_pos = con->scrollback_pos * VGA_WIDTH;
	for (size_t x = 0; x < VGA_WIDTH; x++)
	{
		con->scrollback[save_pos + x] = con->shadow[x];
	}

	/* スクロールバックバッファの位置を更新（リングバッファ） */
	con->scrollback_pos = (con->scrollback_pos + 1) % SCROLLBACK_LINES;
	if (con->scrollback_lines < SCROLLBACK_LINES)
	{
		con->scrollback_lines++;
	}

	/* VGAに書き込んだ各行を1行上に上げる */
	int flush_hw = console_is_active(con) && kfs_terminal_buffer;
	for (size_t y = 1; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			uint16_t value = con->shadow[y * VGA_WIDTH + x];
			con->shadow[(y - 1) * VGA_WIDTH + x] = value;
			if (flush_hw)
			{
				kfs_terminal_buffer[(y - 1) * VGA_WIDTH + x] = value;
			}
		}
	}

	/* 最後の行を空白で埋める */
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	for (size_t x = 0; x < VGA_WIDTH; x++)
	{
		con->shadow[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
		if (flush_hw)
		{
			kfs_terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
		}
	}
	con->row = VGA_HEIGHT - 1;

	/* 新しい出力があったらスクロールオフセットをリセット */
	con->scroll_offset = 0;
}

/* 値cを現在のコンソールに出力する(MMIO) */
void terminal_putchar(char c)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	console_activate_if_needed(con);
	if (c == '\n')
	{
		con->column = 0; /* キャリッジリターン */
		con->row++;		 /* ラインフィード */
		terminal_scroll_if_needed(con);
		sync_globals_from_console(con);
		return;
	}
	if (c == '\r')
	{
		con->column = 0; /* キャリッジリターン */
		sync_globals_from_console(con);
		return;
	}
	/* 挿入モード: カーソル位置に文字を挿入 */
	terminal_insert_char_at(con, c, con->column, con->row);
	if (++con->column == VGA_WIDTH)
	{
		con->column = 0;
		con->row++;
	}
	terminal_scroll_if_needed(con);
	sync_globals_from_console(con);
}

/* 上書きモードで文字を出力（バックスペース用） */
void terminal_putchar_overwrite(char c)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	console_activate_if_needed(con);
	terminal_putentryat(con, c, con->column, con->row);
	sync_globals_from_console(con);
}

/* カーソル位置の文字を削除し、右側の文字を左にシフト（バックスペース用） */
void terminal_delete_char(void)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	console_activate_if_needed(con);

	size_t x = con->column;
	size_t y = con->row;

	/* カーソル位置から行末まで左にシフト */
	for (size_t i = x; i < VGA_WIDTH - 1; i++)
	{
		con->shadow[y * VGA_WIDTH + i] = con->shadow[y * VGA_WIDTH + i + 1];
		if (console_is_active(con) && kfs_terminal_buffer)
		{
			kfs_terminal_buffer[y * VGA_WIDTH + i] = con->shadow[y * VGA_WIDTH + i];
		}
	}

	/* 行末を空白で埋める */
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	con->shadow[y * VGA_WIDTH + (VGA_WIDTH - 1)] = blank;
	if (console_is_active(con) && kfs_terminal_buffer)
	{
		kfs_terminal_buffer[y * VGA_WIDTH + (VGA_WIDTH - 1)] = blank;
	}

	sync_globals_from_console(con);
}

/* メモリに値dataをsizeだけ書き込む(MMIO) */
void terminal_write(const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		terminal_putchar(data[i]);
	}
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
	{
		row = VGA_HEIGHT - 1;
	}
	if (column >= VGA_WIDTH)
	{
		column = VGA_WIDTH - 1;
	}
	con->row = row;
	con->column = column;
	sync_globals_from_console(con);
}

void kfs_terminal_get_cursor(size_t *row, size_t *column)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	if (row)
	{
		*row = con->row;
	}
	if (column)
	{
		*column = con->column;
	}
}

/* 現在使用してるコンソールで出力する文字の色を設定 */
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

/* 仮想コンソールの数を取得 */
size_t kfs_terminal_console_count(void)
{
	return KFS_VIRTUAL_CONSOLE_COUNT;
}

/* 仮想コンソールをindexで指定したものに切り替える */
void kfs_terminal_switch_console(size_t index)
{
	ensure_console_bootstrap();
	if (index >= KFS_VIRTUAL_CONSOLE_COUNT || index == kfs_console_active)
	{
		return;
	}
	struct kfs_console_state *current = active_console();
	if (current->initialized)
	{
		console_capture_from_hw(current);
	}
	kfs_console_active = index;
	struct kfs_console_state *next = active_console();
	console_activate_if_needed(next);
	console_flush_to_hw(next);
	sync_globals_from_console(next);
}

/* スクロールバックバッファを使って画面を再描画 */
static void redraw_with_scroll_offset(struct kfs_console_state *con)
{
	if (!kfs_terminal_buffer || !console_is_active(con))
	{
		return;
	}

	if (con->scroll_offset == 0)
	{
		/* オフセット0の場合は通常のshadowバッファを表示 */
		console_flush_to_hw(con);
		return;
	}

	/* スクロールオフセットをクランプ */
	int offset = con->scroll_offset;
	if (offset > (int)con->scrollback_lines)
	{
		offset = (int)con->scrollback_lines;
	}
	if (offset > (int)VGA_HEIGHT)
	{
		offset = (int)VGA_HEIGHT;
	}

	/* スクロールバックから何行表示するか */
	int lines_from_scrollback = offset;

	/* 現在のshadowから何行表示するか */
	int lines_from_shadow = VGA_HEIGHT - offset;
	if (lines_from_shadow < 0)
	{
		lines_from_shadow = 0;
	}

	/* スクロールバックバッファの読み取り開始位置を計算 */
	/* scrollback_posは次に書き込む位置 */
	/*
	 * 例: 2行保存されている場合
	 *   scrollback_pos = 2, scrollback_lines = 2
	 *   保存されている行: インデックス 0 (古い), 1 (新しい)
	 *   offset = 1 なら、インデックス 1 から表示開始
	 *   offset = 2 なら、インデックス 0 から表示開始
	 */
	size_t scrollback_read_pos;
	if (con->scrollback_lines < SCROLLBACK_LINES)
	{
		/* まだバッファが一杯でない場合 */
		/* scrollback_linesは保存されている行数 */
		/* scrollback_posは次に書き込む位置 = scrollback_lines */
		/* offset行スクロールアップするとき、(scrollback_pos - offset)の位置から読む */
		if (offset <= (int)con->scrollback_pos)
		{
			scrollback_read_pos = con->scrollback_pos - offset;
		}
		else
		{
			scrollback_read_pos = 0; /* オフセットが大きすぎる場合は最古の行から */
		}
	}
	else
	{
		/* バッファが一杯の場合（リングバッファ） */
		/* scrollback_posは次に書き込む位置 = 最古の行の位置 */
		/* 最新の行は (scrollback_pos - 1 + SCROLLBACK_LINES) % SCROLLBACK_LINES */
		/* offset行前は (scrollback_pos - offset + SCROLLBACK_LINES) % SCROLLBACK_LINES */
		scrollback_read_pos = (con->scrollback_pos - offset + SCROLLBACK_LINES) % SCROLLBACK_LINES;
	}

	/* 画面を再描画 */
	size_t screen_line = 0;

	/* スクロールバックバッファから表示 */
	for (int i = 0; i < lines_from_scrollback; i++)
	{
		size_t buf_line = (scrollback_read_pos + i) % SCROLLBACK_LINES;
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			kfs_terminal_buffer[screen_line * VGA_WIDTH + x] = con->scrollback[buf_line * VGA_WIDTH + x];
		}
		screen_line++;
	}

	/* 残りは現在のshadowバッファから表示 */
	for (int i = 0; i < lines_from_shadow; i++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			kfs_terminal_buffer[screen_line * VGA_WIDTH + x] = con->shadow[i * VGA_WIDTH + x];
		}
		screen_line++;
	}
}

/* スクロールアップ（過去の内容を表示） */
void kfs_terminal_scroll_up(void)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();

	/* スクロールバックバッファに保存されている行数まで */
	if (con->scroll_offset < (int)con->scrollback_lines)
	{
		con->scroll_offset++;
		redraw_with_scroll_offset(con);
	}
}

/* スクロールダウン（最新の内容に戻る） */
void kfs_terminal_scroll_down(void)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();

	if (con->scroll_offset > 0)
	{
		con->scroll_offset--;
		redraw_with_scroll_offset(con);
	}
}

/* 左矢印キー: カーソルを左に移動 */
void kfs_terminal_cursor_left(void)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();

	/* スクロール中は無効 */
	if (con->scroll_offset != 0)
	{
		return;
	}

	if (con->column > 0)
	{
		con->column--;
	}
	else if (con->row > 0)
	{
		/* 前の行の末尾に移動 */
		con->row--;
		con->column = VGA_WIDTH - 1;
	}
	sync_globals_from_console(con);
	update_hardware_cursor();
}

/* 右矢印キー: カーソルを右に移動 */
void kfs_terminal_cursor_right(void)
{
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();

	/* スクロール中は無効 */
	if (con->scroll_offset != 0)
	{
		return;
	}

	if (con->column < VGA_WIDTH - 1)
	{
		con->column++;
	}
	else if (con->row < VGA_HEIGHT - 1)
	{
		/* 次の行の先頭に移動 */
		con->row++;
		con->column = 0;
	}
	sync_globals_from_console(con);
	update_hardware_cursor();
}
