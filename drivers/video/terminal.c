#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/pid.h>
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
#define VGA_CURSOR_START 0x0A
#define VGA_CURSOR_END 0x0B
#define SCROLLBACK_LINES 100 /* スクロールバックバッファの行数 */
#define ANSI_MAX_PARAMS 8	 /* ANSIエスケープシーケンスの最大パラメータ数 */
#define VGA_TAB_WIDTH 8		 /* タブ文字を展開する桁幅 */

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
	uint8_t ansi_state;				  /* ANSIエスケープシーケンスのパーサ状態 */
	int ansi_params[ANSI_MAX_PARAMS]; /* ANSIエスケープシーケンスのパラメータ配列 */
	int ansi_param_count;			  /* 現在解析中のパラメータ数 */
	int ansi_current;				  /* 現在解析中の数値 */
	int ansi_has_current;			  /* 現在解析中の数値があるかどうか */
	uint16_t shadow[VGA_WIDTH * VGA_HEIGHT];
	uint16_t scrollback[SCROLLBACK_LINES * VGA_WIDTH]; /* スクロールバックバッファ */
	size_t scrollback_pos;	 /* スクロールバックバッファ内の現在位置（リングバッファ） */
	size_t scrollback_lines; /* 保存されているスクロールバック行数 */
	int scroll_offset;		 /* 現在のスクロールオフセット（0=最新、正の値=過去） */
	int initialized;
};

static struct kfs_console_state kfs_console_states[KFS_VIRTUAL_CONSOLE_COUNT];
static size_t kfs_console_active; /* 現在アクティブなコンソールのインデックス */
static pid_t foreground_pgrp_per_console[KFS_VIRTUAL_CONSOLE_COUNT];
pid_t foreground_pgrp; /* 端末のフォアグラウンドプロセスグループID（0=未設定） */
static int kfs_console_bootstrap_completed;

/* 現在使用しているコンソールを取得 */
static struct kfs_console_state *active_console(void)
{
	return &kfs_console_states[kfs_console_active];
}

static int console_is_active(const struct kfs_console_state *con)
{
	return con == &kfs_console_states[kfs_console_active];
}

enum ansi_parse_state
{
	ANSI_STATE_TEXT = 0, /* 通常のテキスト状態 */
	ANSI_STATE_ESC,		 /* ESC文字を受け取った状態 */
	ANSI_STATE_CSI,		 /* CSIシーケンスを受け取った状態 */
};

/** デフォルトの端末色を返す
 * @brief 文字色をライトグレー、背景色を黒に初期化するための色属性を生成する
 * @return VGA色属性（fg=VGA_COLOR_LIGHT_GREY, bg=VGA_COLOR_BLACK）
 */
static uint8_t terminal_default_color(void)
{
	/* デフォルトは「明るい灰色の文字 + 黒背景」。 */
	return kfs_vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/** ANSIエスケープシーケンスのパーサ状態を初期化する
 * @param con コンソール状態
 * @brief ESC解析状態・パラメータ配列の進捗・現在値をすべて初期状態に戻す
 */
static void ansi_reset_parser(struct kfs_console_state *con)
{
	/* ESC シーケンス解析状態を初期状態へ戻す。 */
	con->ansi_state = ANSI_STATE_TEXT;
	con->ansi_param_count = 0;
	con->ansi_current = 0;
	con->ansi_has_current = 0;
}

/** ANSI基本8色インデックスをVGA色定数へ変換する
 * @param idx ANSI基本色インデックス（0-7）
 * @return 対応するVGA色（範囲外はVGA_COLOR_LIGHT_GREY）
 * @note 0=black, 1=red, 2=green, 3=brown, 4=blue, 5=magenta, 6=cyan, 7=light grey
 */
static enum vga_color ansi_basic_color_to_vga(int idx)
{
	/* ANSI 基本8色(0-7)を VGA 色定数へ写像する。 */
	switch (idx)
	{
	case 0:
		return VGA_COLOR_BLACK;
	case 1:
		return VGA_COLOR_RED;
	case 2:
		return VGA_COLOR_GREEN;
	case 3:
		return VGA_COLOR_BROWN;
	case 4:
		return VGA_COLOR_BLUE;
	case 5:
		return VGA_COLOR_MAGENTA;
	case 6:
		return VGA_COLOR_CYAN;
	default:
		return VGA_COLOR_LIGHT_GREY;
	}
}

/** SGRパラメータ列を現在コンソールの色状態へ適用する
 * @param con コンソール状態
 * @param params 解析済みSGRパラメータ配列
 * @param count paramsの要素数
 * @brief ESC[...m で渡された属性（太字/前景色/背景色/リセット）を順次反映する
 * @note 対応コード: 0,1,22,39,49,30-37,40-47,90-97,100-107
 * @ref https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_parameters
 */
static void ansi_apply_sgr(struct kfs_console_state *con, const int *params, int count)
{
	/* 現在色を基準に SGR パラメータを順に適用する。 */
	enum vga_color fg = (enum vga_color)(con->color & 0x0F);
	enum vga_color bg = (enum vga_color)((con->color >> 4) & 0x0F);
	int bright_fg = (fg >= VGA_COLOR_DARK_GREY);

	for (int i = 0; i < count; i++)
	{
		int p = params[i];

		if (p == 0) /* リセット */
		{
			fg = VGA_COLOR_LIGHT_GREY;
			bg = VGA_COLOR_BLACK;
			bright_fg = 0;
			continue;
		}
		if (p == 1) /* 太字（明るい色） */
		{
			bright_fg = 1;
			if (fg <= VGA_COLOR_LIGHT_GREY)
			{
				fg = (enum vga_color)(fg + 8);
			}
			continue;
		}
		if (p == 22) /* 太字（明るい色）を解除 */
		{
			bright_fg = 0;
			if (fg >= VGA_COLOR_DARK_GREY)
			{
				fg = (enum vga_color)(fg - 8);
			}
			continue;
		}
		if (p == 39) /* 文字色をデフォルトに戻す */
		{
			/* 文字色だけデフォルトに戻す。 */
			fg = VGA_COLOR_LIGHT_GREY;
			if (bright_fg && fg <= VGA_COLOR_LIGHT_GREY)
			{
				fg = (enum vga_color)(fg + 8);
			}
			continue;
		}
		if (p == 49) /* 背景色をデフォルトに戻す */
		{
			/* 背景色だけデフォルトに戻す。 */
			bg = VGA_COLOR_BLACK;
			continue;
		}
		if (p >= 30 && p <= 37) /* 文字色を設定 */
		{
			fg = ansi_basic_color_to_vga(p - 30);
			if (bright_fg)
			{
				fg = (enum vga_color)(fg + 8);
			}
			continue;
		}
		if (p >= 40 && p <= 47) /* 背景色を設定 */
		{
			bg = ansi_basic_color_to_vga(p - 40);
			continue;
		}
		if (p >= 90 && p <= 97) /* 明るい文字色を設定 */
		{
			fg = (enum vga_color)(ansi_basic_color_to_vga(p - 90) + 8);
			bright_fg = 1;
			continue;
		}
		if (p >= 100 && p <= 107) /* 明るい背景色を設定 */
		{
			bg = (enum vga_color)(ansi_basic_color_to_vga(p - 100) + 8);
			continue;
		}
	}

	con->color = kfs_vga_make_color(fg, bg);
	kfs_terminal_color = con->color;
}

/** 現在解析中のCSIパラメータを配列へ確定する
 * @brief con->ansi_currentに現在解析中の数値があれば
 *        con->ansi_paramsへ追加し、解析状態をリセットする
 * @param con コンソール状態
 * @return 0: 成功，-1: パラメータ上限超過
 */
static int ansi_push_current_param(struct kfs_console_state *con)
{
	/* パラメータ上限を超えた場合はエラーを返す */
	if (con->ansi_param_count >= ANSI_MAX_PARAMS)
	{
		return -1;
	}

	if (con->ansi_has_current)
	{
		/* 現在解析中の数値があれば配列へ追加する */
		con->ansi_params[con->ansi_param_count++] = con->ansi_current;
	}
	else
	{
		/* 現在解析中の数値がなければデフォルト値0を追加する */
		con->ansi_params[con->ansi_param_count++] = 0;
	}

	con->ansi_current = 0;
	con->ansi_has_current = 0;
	return 0;
}

/** ESC[...m のうち SGR（色）を最小実装で解釈する
 * @param con コンソール状態
 * @param c   入力文字（ESC[...m のうち ...の部分の1文字）
 * @return ANSIエスケープシーケンスの一部として処理した場合は1、そうでなければ0
 * @note SGR以外のシーケンスは解釈せずにリセットする（ESC[...m のうち
 * ...の部分の1文字目が数字でも';'でも'm'でもない場合はリセットする）
 * @example
 * conが"ESC[31m"を受け取ると，
 * c='3'のときにパラメータ31を解析し，
 * c='1'のときにパラメータ1を解析し，
 * c='m'のときにSGRを適用して文字色を赤にする
 */
static int terminal_try_handle_escape(struct kfs_console_state *con, char c)
{
	/* コンソールがテキスト状態である場合 */
	if (con->ansi_state == ANSI_STATE_TEXT)
	{
		/* cがESC文字(0x1B)の場合 */
		if ((unsigned char)c == 0x1B)
		{
			/* コンソールの状態をESCに変更する */
			con->ansi_state = ANSI_STATE_ESC;
			return 1;
		}
		return 0;
	}

	/* コンソールの状態がESCである場合 */
	if (con->ansi_state == ANSI_STATE_ESC)
	{
		if (c == '[')
		{
			/** コンソールの状態をCSIに変更する．
			 * @brief CSI[Control Sequence Introducer]とは，
			 *        ESC[で始まるANSIエスケープシーケンスのうち，
			 *        パラメータを取るものの開始を示す文字である
			 *        たとえば，"ESC[31m"における"ESC["をCSIと呼ぶ．
			 */
			con->ansi_state = ANSI_STATE_CSI;
			con->ansi_param_count = 0;
			con->ansi_current = 0;
			con->ansi_has_current = 0;
			return 1;
		}
		ansi_reset_parser(con);
		return 0;
	}

	/* コンソールの状態がCSIである場合 */
	if (c >= '0' && c <= '9')
	{
		/** パラメータの数字を解析する
		 * @example "ESC[31m"において，'3'を受け取ったときにansi_currentを3にし，
		 *          次に'1'を受け取ったときにansi_currentを31にする
		 */
		con->ansi_current = con->ansi_current * 10 + (c - '0');
		con->ansi_has_current = 1;
		return 1;
	}

	/* パラメータ区切り文字 ';' を処理する */
	if (c == ';')
	{
		if (ansi_push_current_param(con) < 0)
		{
			ansi_reset_parser(con);
		}
		return 1;
	}

	/* SGR（Select Graphic Rendition）シーケンスを処理する */
	if (c == 'm')
	{
		if (con->ansi_has_current)
		{
			if (ansi_push_current_param(con) < 0)
			{
				ansi_reset_parser(con);
				return 1;
			}
		}
		else if (con->ansi_param_count == 0)
		{
			con->ansi_params[con->ansi_param_count++] = 0;
		}
		ansi_apply_sgr(con, con->ansi_params, con->ansi_param_count);
		ansi_reset_parser(con);
		return 1;
	}

	ansi_reset_parser(con);
	return 0;
}

/** カーソルの形状
 * @note VGAテキストモードでは各文字は高さ16ピクセル（スキャンライン0〜15）で構成される．
 *       カーソルの形状は表示するスキャンラインの範囲で決定する:
 *       - 0-1:   文字の最上部2本 → 細い縦線
 *       - 14-15: 文字の最下部2本 → アンダースコア
 *       - 0-15:  文字全体を覆う  → ブロック
 */
enum cursor_shape
{
	CURSOR_UNDERLINE, /* アンダースコア (14-15) */
	CURSOR_BLOCK,	  /* ブロック (0-15) */
	CURSOR_VERTICAL,  /* 縦線 (0-1) */
};

/** カーソルの形状を設定する
 * @param shape カーソルの形状
 * @note VGAハードウェアではカーソルの点滅は常に有効であり，
 *       点滅を無効にすることはできない。
 */
static void kfs_terminal_set_cursor_shape(enum cursor_shape shape)
{
	uint8_t start, end;

	switch (shape)
	{
	case CURSOR_BLOCK:
		start = 0;
		end = 15;
		break;
	case CURSOR_VERTICAL:
		start = 0;
		end = 1;
		break;
	case CURSOR_UNDERLINE:
	default:
		start = 14;
		end = 15;
		break;
	}
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, VGA_CURSOR_START);
	kfs_io_outb(VGA_CRTC_DATA_PORT, start);
	kfs_io_outb(VGA_CRTC_COMMAND_PORT, VGA_CURSOR_END);
	kfs_io_outb(VGA_CRTC_DATA_PORT, end);
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
		ansi_reset_parser(con);
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
		foreground_pgrp_per_console[i] = 0;
	}
	kfs_console_active = 0;
	foreground_pgrp = foreground_pgrp_per_console[kfs_console_active];
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
		con->color = terminal_default_color();
		ansi_reset_parser(con);
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
	con->color = terminal_default_color();
	con->row = 0;	 /* 0行目(直感的には画面最上部)から記述を開始する */
	con->column = 0; /* 0列目(直感的には画面最左部)から記述を開始する */
	ansi_reset_parser(con);
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

	/* デフォルトのカーソルの形状を設定する */
	kfs_terminal_set_cursor_shape(CURSOR_BLOCK);
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

/* 値cをコンソールconに出力する(MMIO) */
static void terminal_putchar(struct kfs_console_state *con, char c)
{
	if (c == '\n')
	{
		con->column = 0; /* キャリッジリターン */
		con->row++;		 /* ラインフィード */
		terminal_scroll_if_needed(con);
		if (console_is_active(con))
		{
			sync_globals_from_console(con);
		}
		return;
	}
	if (c == '\r')
	{
		con->column = 0; /* キャリッジリターン */
		if (console_is_active(con))
		{
			sync_globals_from_console(con);
		}
		return;
	}
	if (c == '\t')
	{
		/** タブ文字の場合，次のタブストップまで移動する
		 * @details 計算方法は，現在の列番号をVGA_TAB_WIDTHで割った余りを引いて，
		 *          次のタブストップまでの空文字数を求める．
		 *          VGA_TAB_WIDTHが8の場合，次のタブストップは8である．
		 *
		 * @example |ABC     |         現在の列が3のとき，
		 *              ^^^^^          空文字数は8-(3%8)=5となる．
		 *
		 * @example |ABCDEFGH        | 現在の列が8のとき，
		 *                   ^^^^^^^^  空文字数は8-(8%8)=8となる．
		 *
		 * @example |ABCDEFGHABCDEFG | 現在の列が15のとき，
		 *                          ^  空文字数は8-(15%8)=1となる．
		 *
		 */
		size_t spaces = VGA_TAB_WIDTH - (con->column % VGA_TAB_WIDTH);

		for (size_t i = 0; i < spaces; i++)
		{
			terminal_putchar(con, ' ');
		}
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
	if (console_is_active(con))
	{
		sync_globals_from_console(con);
	}
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
	ensure_console_bootstrap();
	struct kfs_console_state *con = active_console();
	console_activate_if_needed(con);

	for (size_t i = 0; i < size; i++)
	{
		if (!terminal_try_handle_escape(con, data[i]))
		{
			terminal_putchar(con, data[i]);
		}
	}
}

/* index指定のコンソールのメモリに値dataをsizeだけ書き込む(MMIO) */
void terminal_write_console(size_t index, const char *data, size_t size)
{
	ensure_console_bootstrap();
	if (index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return;
	}

	struct kfs_console_state *con = &kfs_console_states[index];
	console_activate_if_needed(con);

	for (size_t i = 0; i < size; i++)
	{
		if (!terminal_try_handle_escape(con, data[i]))
		{
			terminal_putchar(con, data[i]);
		}
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

/* 現在アクティブなコンソールのインデックスを返す */
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

	/* 指定されたインデックスが有効かどうかを確認 */
	if (index >= KFS_VIRTUAL_CONSOLE_COUNT || index == kfs_console_active)
	{
		return;
	}

	/* 切り替え前（現在）のコンソールを取得する */
	struct kfs_console_state *current = active_console();
	if (current->initialized)
	{
		console_capture_from_hw(current);
	}

	/* 現在アクティブなコンソールを切り替える */
	kfs_console_active = index;
	struct kfs_console_state *next = active_console();
	console_activate_if_needed(next);
	console_flush_to_hw(next);
	sync_globals_from_console(next);

	/* アクティブなコンソールのフォアグラウンドプロセスグループを更新する */
	foreground_pgrp = foreground_pgrp_per_console[kfs_console_active];
}

/* 指定したコンソールのフォアグラウンドプロセスグループを取得する */
pid_t kfs_terminal_get_foreground_pgrp_for_console(size_t index)
{
	ensure_console_bootstrap();
	if (index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return 0;
	}
	return foreground_pgrp_per_console[index];
}

/** 指定したコンソールのフォアグラウンドプロセスグループを設定する
 * @param index コンソールのインデックス
 * @param pgrp プロセスグループID
 * @return 0: 成功, -EINVAL: indexが不正
 */
int kfs_terminal_set_foreground_pgrp_for_console(size_t index, pid_t pgrp)
{
	ensure_console_bootstrap();

	if (index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return -EINVAL;
	}

	/* 指定したコンソールのフォアグラウンドプロセスグループを設定する．
	 * これにより，Ctrl-Cなどのシグナルがそのプロセスグループに送られるようになる */
	foreground_pgrp_per_console[index] = pgrp;

	/* 指定したコンソールがアクティブな場合は，
	 * グローバルのforeground_pgrpも更新する */
	if (index == kfs_console_active)
	{
		foreground_pgrp = pgrp;
	}

	return 0;
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
