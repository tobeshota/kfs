#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/multiboot.h>
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
#define SCROLLBACK_LINES 100	  /* スクロールバックバッファの行数 */
#define ANSI_MAX_PARAMS 8		  /* ANSIエスケープシーケンスの最大パラメータ数 */
#define VGA_TAB_WIDTH 8			  /* タブ文字を展開する桁幅 */
#define VGA_COLOR_INDEX_MASK 0x0F /* VGAの色属性バイトから4bitの色番号を取り出すためのマスク */
#define VGA_COLOR_PALETTE_SIZE (VGA_COLOR_INDEX_MASK + 1) /* VGAのカラーパレットのサイズ */
#define VGA_COLOR_BACKGROUND_SHIFT 4 /* VGAの属性バイトから背景色を取り出すためのシフト量 */
#define KFS_FB_VADDR 0xE0000000UL	 /* フレームバッファの仮想アドレス */
#define KFS_FB_MAX_TABLES 8 /* フレームバッファマッピングに使用するページテーブルの最大数（32MBまで対応） */
#define KFS_FB_CELL_WIDTH 8	  /* フレームバッファのセルの幅（ピクセル単位） */
#define KFS_FB_CELL_HEIGHT 16 /* フレームバッファのセルの高さ（ピクセル単位） */

extern void kfs_io_outb(uint16_t port, uint8_t val);
extern pde_t boot_page_directory[]; /* ブート時のページディレクトリ(boot.Sで定義) */

size_t kfs_terminal_row;
size_t kfs_terminal_column;
uint8_t kfs_terminal_color;
uint16_t *kfs_terminal_buffer = (uint16_t *)VGA_MEMORY; /* 画面に文字を書き込むアドレスのラッパ */

struct kfs_console_state
{
	size_t row;
	size_t column;
	uint8_t color;
	uint8_t ansi_state;						 /* ANSIエスケープシーケンスのパーサ状態 */
	int ansi_params[ANSI_MAX_PARAMS];		 /* ANSIエスケープシーケンスのパラメータ配列 */
	int ansi_param_count;					 /* 現在解析中のパラメータ数 */
	int ansi_current;						 /* 現在解析中の数値 */
	int ansi_has_current;					 /* 現在解析中の数値があるかどうか */
	uint16_t shadow[VGA_WIDTH * VGA_HEIGHT]; /* シャドウバッファ（端末のテキスト内容を保持） */
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

/* フレームバッファの状態を保持する構造体 */
struct kfs_framebuffer_state
{
	uint8_t *base;		/* フレームバッファのベースアドレス */
	uint32_t pitch;		/* フレームバッファの1行あたりのバイト数 */
	uint32_t width;		/* フレームバッファの幅（ピクセル単位） */
	uint32_t height;	/* フレームバッファの高さ（ピクセル単位） */
	uint8_t bpp;		/* ビット深度（bits per pixel） */
	uint8_t red_pos;	/* 赤成分のビット位置 */
	uint8_t red_size;	/* 赤成分のビットサイズ */
	uint8_t green_pos;	/* 緑成分のビット位置 */
	uint8_t green_size; /* 緑成分のビットサイズ */
	uint8_t blue_pos;	/* 青成分のビット位置 */
	uint8_t blue_size;	/* 青成分のビットサイズ */
	int enabled;		/* フレームバッファが有効かどうか */
};

static struct kfs_framebuffer_state kfs_framebuffer;
static pte_t kfs_framebuffer_tables[KFS_FB_MAX_TABLES][PTRS_PER_PTE] __attribute__((aligned(PAGE_SIZE)));

/* 現在使用しているコンソールを取得 */
static struct kfs_console_state *active_console(void)
{
	return &kfs_console_states[kfs_console_active];
}

/* 現在のコンソールがアクティブかどうかを判定する */
static int console_is_active(const struct kfs_console_state *con)
{
	return con == &kfs_console_states[kfs_console_active];
}

/* 指定値valueを指定アラインメントalignに切り下げる */
static unsigned long align_down_ulong(unsigned long value, unsigned long align)
{
	return value & ~(align - 1);
}

/* 指定値valueを指定アラインメントalignに切り上げる */
static unsigned long align_up_ulong(unsigned long value, unsigned long align)
{
	return (value + align - 1) & ~(align - 1);
}

/** 指定値valueを指定ビット数bitsに合わせて調整する
 * @param value 調整する値（0-255）
 * @param bits 調整後のビット数（0-8）
 * @return bitsビットに合わせて調整された値
 * @example
 * value=255, bits=5の場合，255は8ビットで11111111のため，これを5ビットに合わせた11111（31）を返す．
 * value=128, bits=4の場合，128は8ビットで10000000のため，これを4ビットに合わせた1000（8）を返す．
 */
static unsigned long framebuffer_color_component(uint8_t value, uint8_t bits)
{
	if (bits >= 8)
	{
		return value;
	}
	if (bits == 0)
	{
		return 0;
	}

	return value >> (8 - bits);
}

/** 指定されたRGB値をフレームバッファのカラーフォーマットにパックする
 * @param r 赤成分の値（0-255）
 * @param g 緑成分の値（0-255）
 * @param b 青成分の値（0-255）
 * @return フレームバッファのカラーフォーマットにパックされた色値
 * @note ここでパックするとは，
 *       フレームバッファのビット配置に合わせてRGBの各成分を適切な位置に配置し，
 *       1つの整数値としてまとめることを意味する．
 */
static unsigned long framebuffer_pack_color(uint8_t r, uint8_t g, uint8_t b)
{
	unsigned long color = 0;

	color |= framebuffer_color_component(r, kfs_framebuffer.red_size) << kfs_framebuffer.red_pos;
	color |= framebuffer_color_component(g, kfs_framebuffer.green_size) << kfs_framebuffer.green_pos;
	color |= framebuffer_color_component(b, kfs_framebuffer.blue_size) << kfs_framebuffer.blue_pos;
	return color;
}

/** 指定されたVGAカラーパレットの色をRGB値に変換する
 * @param color VGAカラーパレットの色（0-15）
 * @param r 赤成分の値（0-255）へのポインタ
 * @param g 緑成分の値（0-255）へのポインタ
 * @param b 青成分の値（0-255）へのポインタ
 */
static void framebuffer_vga_color(uint8_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
	/* VGAカラーパレットの色をRGB値に変換するための配列 */
	static const uint8_t vga_color_palette[VGA_COLOR_PALETTE_SIZE][3] = {
		{0x00, 0x00, 0x00}, {0x00, 0x00, 0xaa}, {0x00, 0xaa, 0x00}, {0x00, 0xaa, 0xaa},
		{0xaa, 0x00, 0x00}, {0xaa, 0x00, 0xaa}, {0xaa, 0x55, 0x00}, {0xaa, 0xaa, 0xaa},
		{0x55, 0x55, 0x55}, {0x55, 0x55, 0xff}, {0x55, 0xff, 0x55}, {0x55, 0xff, 0xff},
		{0xff, 0x55, 0x55}, {0xff, 0x55, 0xff}, {0xff, 0xff, 0x55}, {0xff, 0xff, 0xff},
	};

	*r = vga_color_palette[color & VGA_COLOR_INDEX_MASK][0];
	*g = vga_color_palette[color & VGA_COLOR_INDEX_MASK][1];
	*b = vga_color_palette[color & VGA_COLOR_INDEX_MASK][2];
}

/** 指定文字cの指定行rowに対応するフォントデータを取得する
 * @param c 文字
 * @param row 行番号（0-6）
 * @return フォントデータ
 * @example font5x7_row('A', 0)は'A'のフォントデータの0行目を返す
 */
static uint8_t font5x7_row(char c, int row)
{
	/* 5x7フォントの数字データ */
	static const uint8_t digits[10][7] = {
		{0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}, {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
		{0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}, {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
		{0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}, {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
		{0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}, {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
		{0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e},
	};
	/* 5x7フォントの大文字アルファベットデータ */
	static const uint8_t uppercase_letters[26][7] = {
		{0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
		{0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}, {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
		{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}, {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
		{0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f}, {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
		{0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}, {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c},
		{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
		{0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}, {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
		{0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
		{0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}, {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
		{0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}, {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
		{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04},
		{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}, {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
		{0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}, {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f},
	};
	/* 5x7フォントの小文字アルファベットデータ */
	static const uint8_t lowercase_letters[26][7] = {
		{0x00, 0x00, 0x0e, 0x01, 0x0f, 0x11, 0x0f}, {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x1e},
		{0x00, 0x00, 0x0e, 0x10, 0x10, 0x10, 0x0e}, {0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x0f},
		{0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0e}, {0x06, 0x09, 0x08, 0x1c, 0x08, 0x08, 0x08},
		{0x00, 0x00, 0x0f, 0x11, 0x11, 0x0f, 0x01}, {0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x11},
		{0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x0e}, {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c},
		{0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12}, {0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e},
		{0x00, 0x00, 0x1a, 0x15, 0x15, 0x15, 0x15}, {0x00, 0x00, 0x1e, 0x11, 0x11, 0x11, 0x11},
		{0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e}, {0x00, 0x00, 0x1e, 0x11, 0x11, 0x1e, 0x10},
		{0x00, 0x00, 0x0f, 0x11, 0x11, 0x0f, 0x01}, {0x00, 0x00, 0x16, 0x18, 0x10, 0x10, 0x10},
		{0x00, 0x00, 0x0f, 0x10, 0x0e, 0x01, 0x1e}, {0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06},
		{0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d}, {0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04},
		{0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a}, {0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11},
		{0x00, 0x00, 0x11, 0x11, 0x11, 0x0f, 0x01}, {0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f},
	};

	/* 行番号が範囲外の場合は0（' 'と同じ）を返す */
	if (row < 0 || row >= 7)
	{
		return 0;
	}

	if (c >= 'a' && c <= 'z')
	{
		return lowercase_letters[c - 'a'][row];
	}
	if (c >= 'A' && c <= 'Z')
	{
		return uppercase_letters[c - 'A'][row];
	}
	if (c >= '0' && c <= '9')
	{
		return digits[c - '0'][row];
	}

	switch (c)
	{
	case ' ':
		return 0x00;
	case '$': {
		/** '$'のフォントデータ
		 * @details
		 * '$'のフォントデータを描画すると次のようになる：
		 * 0x04 -> 00100
		 * 0x0f -> 01111
		 * 0x14 -> 10100
		 * 0x0e -> 01110
		 * 0x05 -> 00101
		 * 0x1e -> 11110
		 * 0x04 -> 00100
		 */
		static const uint8_t g[7] = {0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04};
		return g[row];
	}
	case '#': {
		/** '#'のフォントデータ
		 * @details
		 * '#'のフォントデータを描画すると次のようになる：
		 * 0x0a -> 01010
		 * 0x0a -> 01010
		 * 0x1f -> 11111
		 * 0x0a -> 01010
		 * 0x1f -> 11111
		 * 0x0a -> 01010
		 * 0x0a -> 01010
		 */
		static const uint8_t g[7] = {0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a};
		return g[row];
	}
	case '%': {
		/** '%'のフォントデータ
		 * @details
		 * '%'のフォントデータを描画すると次のようになる：
		 * 0x18 -> 11000
		 * 0x19 -> 11001
		 * 0x02 -> 00010
		 * 0x04 -> 00100
		 * 0x08 -> 01000
		 * 0x13 -> 10011
		 * 0x03 -> 00011
		 */
		static const uint8_t g[7] = {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
		return g[row];
	}
	case '&': {
		/** '&'のフォントデータ
		 * @details
		 * '&'のフォントデータを描画すると次のようになる：
		 * 0x0c -> 01100
		 * 0x12 -> 10010
		 * 0x14 -> 10100
		 * 0x08 -> 01000
		 * 0x15 -> 10101
		 * 0x12 -> 10010
		 * 0x0d -> 01101
		 */
		static const uint8_t g[7] = {0x0c, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0d};
		return g[row];
	}
	case '*': {
		/** '*'のフォントデータ
		 * @details
		 * '*'のフォントデータを描画すると次のようになる：
		 * 0x00 -> 00000
		 * 0x04 -> 00100
		 * 0x15 -> 10101
		 * 0x0e -> 01110
		 * 0x15 -> 10101
		 * 0x04 -> 00100
		 * 0x00 -> 00000
		 */
		static const uint8_t g[7] = {0x00, 0x04, 0x15, 0x0e, 0x15, 0x04, 0x00};
		return g[row];
	}
	case '+': {
		/** '+'のフォントデータ
		 * @details
		 * '+'のフォントデータを描画すると次のようになる：
		 * 0x00 -> 00000
		 * 0x04 -> 00100
		 * 0x04 -> 00100
		 * 0x1f -> 11111
		 * 0x04 -> 00100
		 * 0x04 -> 00100
		 * 0x00 -> 00000
		 */
		static const uint8_t g[7] = {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00};
		return g[row];
	}
	case '-': {
		static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
		return g[row];
	}
	case '_': {
		static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f};
		return g[row];
	}
	case '=': {
		static const uint8_t g[7] = {0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00};
		return g[row];
	}
	case '/': {
		static const uint8_t g[7] = {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
		return g[row];
	}
	case '\\': {
		static const uint8_t g[7] = {0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01};
		return g[row];
	}
	case '|': {
		static const uint8_t g[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
		return g[row];
	}
	case '.': {
		static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c};
		return g[row];
	}
	case ',': {
		static const uint8_t g[7] = {0x00, 0x00, 0x00, 0x00, 0x0c, 0x04, 0x08};
		return g[row];
	}
	case ':': {
		static const uint8_t g[7] = {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00};
		return g[row];
	}
	case ';': {
		static const uint8_t g[7] = {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x04, 0x08};
		return g[row];
	}
	case '!': {
		/** '!'のフォントデータ
		 * @details
		 * '!'のフォントデータを描画すると次のようになる：
		 * 0x04 -> 00100
		 * 0x04 -> 00100
		 * 0x04 -> 00100
		 * 0x04 -> 00100
		 * 0x04 -> 00100
		 * 0x00 -> 00000
		 * 0x04 -> 00100
		 */
		static const uint8_t g[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
		return g[row];
	}
	case '?': {
		/** '?'のフォントデータ
		 * @details
		 * '?'のフォントデータを描画すると次のようになる：
		 * 0x0e -> 01110
		 * 0x11 -> 10001
		 * 0x01 -> 00001
		 * 0x02 -> 00010
		 * 0x04 -> 00100
		 * 0x00 -> 00000
		 * 0x04 -> 00100
		 */
		static const uint8_t g[7] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
		return g[row];
	}
	case '>': {
		static const uint8_t g[7] = {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10};
		return g[row];
	}
	case '<': {
		static const uint8_t g[7] = {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01};
		return g[row];
	}
	case '[': {
		static const uint8_t g[7] = {0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e};
		return g[row];
	}
	case ']': {
		static const uint8_t g[7] = {0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e};
		return g[row];
	}
	case '(': {
		static const uint8_t g[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
		return g[row];
	}
	case ')': {
		static const uint8_t g[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
		return g[row];
	}
	case '\'': {
		static const uint8_t g[7] = {0x0c, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00};
		return g[row];
	}
	case '"': {
		static const uint8_t g[7] = {0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00};
		return g[row];
	}
	default: {
		/** その他の記号のフォントデータ
		 * @details
		 * 描画すると次のようになる：
		 * 0x1f -> 11111
		 * 0x11 -> 10001
		 * 0x05 -> 00101
		 * 0x02 -> 00010
		 * 0x04 -> 00100
		 * 0x00 -> 00000
		 * 0x04 -> 00100
		 */
		static const uint8_t g[7] = {0x1f, 0x11, 0x05, 0x02, 0x04, 0x00, 0x04};
		return g[row];
	}
	}
}

/** フレームバッファにピクセルを描画する
 * @param x 描画するピクセルのX座標
 * @param y 描画するピクセルのY座標
 * @param color 描画する色（フレームバッファのカラーフォーマットにパックされた値）
 * @details フレームバッファが有効で，指定された座標がフレームバッファの範囲内にある場合に，
 *          指定された色でピクセルを描画する
 */
static void framebuffer_put_pixel(size_t x, size_t y, unsigned long color)
{
	/* フレームバッファが無効または座標が範囲外の場合は描画しない */
	if (!kfs_framebuffer.enabled || x >= kfs_framebuffer.width || y >= kfs_framebuffer.height)
	{
		return;
	}

	/* 書くピクセルのアドレス */
	uint8_t *p = kfs_framebuffer.base + y * kfs_framebuffer.pitch + x * (kfs_framebuffer.bpp / 8);

	/* pの位置に指定色のピクセルを書き込む */
	if (kfs_framebuffer.bpp == 32)
	{
		/* ビット深度が32のビットの場合，
		 * そのままピクセルを書き込む */
		*(uint32_t *)p = (uint32_t)color;
	}
	else if (kfs_framebuffer.bpp == 24)
	{
		/* ビット深度が24のビットの場合，
		 * 0xff(8ビット)でマスクして各色成分を設定してピクセルを書き込む */
		p[0] = (uint8_t)(color & 0xff);
		p[1] = (uint8_t)((color >> 8) & 0xff);
		p[2] = (uint8_t)((color >> 16) & 0xff);
	}
}

/** フレームバッファのセルに文字を描画する
 * @param cell_x 描画するセルのX座標
 * @param cell_y 描画するセルのY座標
 * @param entry 描画する文字と色の情報
 * @details 指定されたセルに文字を描画する
 * @example
 * cell_x = 0, cell_y = 0, entry = 0x1f41の場合，
 * entryの下位8ビットは0x41で，これはASCIIコードで'A'を表す．
 * entryの上位8ビットは0x1fで，これはVGAカラーパレットの色を表す．
 * framebuffer_draw_cell();は，セルの左上隅を基準にして，
 * 文字'A'をVGAカラーパレットの色0x1fで描画する．
 */
static void framebuffer_draw_cell(size_t cell_x, size_t cell_y, uint16_t entry)
{
	const char c = (char)(entry & 0xff);			/* 描画する文字 */
	const uint8_t color = (uint8_t)(entry >> 8);	/* 描画する色 */
	const size_t px0 = cell_x * KFS_FB_CELL_WIDTH;	/* セルの左上隅のX座標 */
	const size_t py0 = cell_y * KFS_FB_CELL_HEIGHT; /* セルの左上隅のY座標 */
	uint8_t fr, fg, fb;								/* 前景色のRGB成分 */
	uint8_t br, bg, bb;								/* 背景色のRGB成分 */
	framebuffer_vga_color(color & VGA_COLOR_INDEX_MASK, &fr, &fg, &fb);
	framebuffer_vga_color((color >> VGA_COLOR_BACKGROUND_SHIFT) & VGA_COLOR_INDEX_MASK, &br, &bg, &bb);
	const unsigned long fg_color = framebuffer_pack_color(fr, fg, fb); /* 前景色のパックされた値 */
	const unsigned long bg_color = framebuffer_pack_color(br, bg, bb); /* 背景色のパックされた値 */

	/* (px0, py0)を起点として，フレームバッファのセルに文字を描画する */
	for (size_t py = 0; py < KFS_FB_CELL_HEIGHT; py++)
	{
		const int font_row = ((int)py - 1) / 2;		   /* フォントの行番号 */
		const uint8_t bits = font5x7_row(c, font_row); /* フォントのビットパターン */
		for (size_t px = 0; px < KFS_FB_CELL_WIDTH; px++)
		{
			const int font_col = (int)px - 1; /* フォントの列番号 */
			const int on = font_col >= 0 && font_col < 5 && (bits & (uint8_t)(1 << (4 - font_col)));
			framebuffer_put_pixel(px0 + px, py0 + py, on ? fg_color : bg_color);
		}
	}
}

/** フレームバッファ上にソフトウェアカーソルを描画する
 * @param con コンソールの状態を表す構造体へのポインタ
 * @details VGAハードウェアカーソルはUEFI framebufferには表示されないため，
 *          現在カーソル位置のセルだけ前景色と背景色を入れ替えて描画する．
 */
static void framebuffer_draw_cursor(const struct kfs_console_state *con)
{
	/* フレームバッファが無効の場合，
	 * または，コンソールがアクティブでない場合，
	 * または，カーソル位置が範囲外の場合，描画しない */
	if (!kfs_framebuffer.enabled || !console_is_active(con) || con->row >= VGA_HEIGHT || con->column >= VGA_WIDTH)
	{
		return;
	}

	size_t index = con->row * VGA_WIDTH + con->column; /* カーソル位置 */
	uint16_t entry = con->shadow[index];			   /* カーソル位置のセルの内容 */
	uint8_t color = (uint8_t)(entry >> 8);			   /* カーソル位置のセルの色 */

	/* カーソル位置のセルの色を反転した色 */
	uint8_t inverted = (uint8_t)((color >> VGA_COLOR_BACKGROUND_SHIFT) |
								 ((color & VGA_COLOR_INDEX_MASK) << VGA_COLOR_BACKGROUND_SHIFT));

	/* カーソル位置のセルを反転描画する */
	framebuffer_draw_cell(con->column, con->row, (uint16_t)(entry & 0x00ff) | ((uint16_t)inverted << 8));
}

/** フレームバッファ上のソフトウェアカーソル位置を更新する
 * @param con コンソールの状態を表す構造体へのポインタ
 * @details 前回カーソルを描画したセルだけをシャドウバッファの内容で戻し，
 *          新しいカーソルセルだけを反転描画する．文字出力ごとの全画面再描画を避けるための高速経路．
 */
static void framebuffer_sync_cursor(const struct kfs_console_state *con)
{
	static const struct kfs_console_state *last_con;
	static size_t last_row;
	static size_t last_column;
	static int last_valid;

	if (!kfs_framebuffer.enabled)
	{
		last_valid = 0;
		return;
	}
	if (last_valid && last_con && console_is_active(last_con) && last_row < VGA_HEIGHT && last_column < VGA_WIDTH)
	{
		framebuffer_draw_cell(last_column, last_row, last_con->shadow[last_row * VGA_WIDTH + last_column]);
	}
	framebuffer_draw_cursor(con);
	last_con = con;
	last_row = con->row;
	last_column = con->column;
	last_valid = console_is_active(con);
}

/** シャドウバッファの内容をフレームバッファに反映する
 * @param con コンソールの状態を表す構造体へのポインタ
 * @details シャドウバッファは，端末のテキスト内容を保持するためのメモリ領域であり，
 *          フレームバッファは，実際に画面に表示されるピクセルデータを保持するメモリ領域である．
 *          この関数は，シャドウバッファの内容をフレームバッファに描画し，最後にカーソルセルを反転描画する．
 */
static void framebuffer_flush_shadow(const struct kfs_console_state *con)
{
	/* フレームバッファが無効な場合は描画しない */
	if (!kfs_framebuffer.enabled)
	{
		return;
	}

	/* シャドウバッファの内容をフレームバッファに描画する */
	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			framebuffer_draw_cell(x, y, con->shadow[y * VGA_WIDTH + x]);
		}
	}
	framebuffer_draw_cursor(con);
}

/** ハードウェアセルに文字を描画する
 * @param con コンソールの状態を表す構造体へのポインタ
 * @param index 描画するセルのインデックス
 * @param entry 描画する文字と色の情報
 * @details 指定されたセルに文字を描画する
 * @example index = 0，entry = 0x1f41の場合，
 *          entryの下位8ビットは0x41で，これはASCIIコードで'A'を表す．
 *          entryの上位8ビットは0x1fで，これはVGAカラーパレットの色を表す．
 *          terminal_write_hw_cell();は，セル(0, 0)に文字'A'をVGAカラーパレットの色0x1fで描画する．
 * @note この関数は，コンソールがアクティブでない場合やフレームバッファが無効な場合は描画を行わない．
 */
static void terminal_write_hw_cell(const struct kfs_console_state *con, size_t index, uint16_t entry)
{
	/* コンソールがアクティブでない場合は描画しない */
	if (!console_is_active(con))
	{
		return;
	}

	/* ターミナルバッファが存在する場合は，
	 * indexで指定されたセルにentryを書き込む */
	if (kfs_terminal_buffer)
	{
		kfs_terminal_buffer[index] = entry;
	}

	if (kfs_framebuffer.enabled)
	{
		framebuffer_draw_cell(index % VGA_WIDTH, index / VGA_WIDTH, entry);
	}
}

/** フレームバッファを物理メモリにマップする
 * @param phys_addr フレームバッファの物理アドレス
 * @param pitch フレームバッファのピッチ（1行あたりのバイト数）
 * @param height フレームバッファの高さ（ピクセル数）
 * @return 成功した場合は0、失敗した場合は-1
 * @example phys_addr = 0xa0000，pitch = 320，height = 200の場合，
 *          フレームバッファが物理アドレス0xa0000から始まり、
 *          1行あたり320バイト、200ピクセルの高さを持つと仮定すると、
 *          framebuffer_map();はフレームバッファを物理メモリにマップし、成功すれば0を返す．
 */
static int framebuffer_map(uint64_t phys_addr, uint32_t pitch, uint32_t height)
{
	const uint64_t end64 = phys_addr + (uint64_t)pitch * height; /* フレームバッファの終了アドレス */

	/* フレームバッファのアドレスが32ビットアドレス空間を超える場合はエラー */
	if (phys_addr > 0xffffffffULL || end64 > 0xffffffffULL)
	{
		return -1;
	}

	unsigned long phys_start =
		align_down_ulong((unsigned long)phys_addr, PAGE_SIZE); /* ページ境界に揃えたフレームバッファの開始アドレス */
	unsigned long phys_end =
		align_up_ulong((unsigned long)end64, PAGE_SIZE); /* ページ境界に揃えたフレームバッファの終了アドレス */
	unsigned long map_size = phys_end - phys_start; /* マップするフレームバッファのサイズ */

	/* マップするフレームバッファのサイズが最大テーブル数を超える場合はエラー */
	if (map_size > KFS_FB_MAX_TABLES * PGDIR_SIZE)
	{
		return -1;
	}

	memset(kfs_framebuffer_tables, 0, sizeof(kfs_framebuffer_tables));
	const unsigned long flags = _PAGE_PRESENT | _PAGE_RW | _PAGE_PCD | _PAGE_PWT;

	/* フレームバッファの各ページに対してページテーブルエントリを設定する */
	for (unsigned long offset = 0; offset < map_size; offset += PAGE_SIZE)
	{
		size_t page = offset / PAGE_SIZE;
		set_pte(&kfs_framebuffer_tables[page / PTRS_PER_PTE][page % PTRS_PER_PTE], phys_start + offset, flags);
	}

	/* フレームバッファの各ページテーブルに対してページディレクトリエントリを設定する */
	for (size_t table = 0; table < KFS_FB_MAX_TABLES && table * PGDIR_SIZE < map_size; table++)
	{
		set_pde(&boot_page_directory[pgd_index(KFS_FB_VADDR) + table], __pa(kfs_framebuffer_tables[table]), flags);
	}

	__flush_tlb();

	/* フレームバッファのベースアドレスを設定する
	 * ベースアドレスは，フレームバッファの物理アドレスを
	 * ページ境界に揃えた開始アドレスからのオフセットを加えた仮想アドレスになる
	 */
	kfs_framebuffer.base = (uint8_t *)(KFS_FB_VADDR + ((unsigned long)phys_addr - phys_start));
	return 0;
}

/** フレームバッファを設定する
 * @param fb Multiboot2のフレームバッファタグへのポインタ
 * @details Multiboot2のフレームバッファタグからフレームバッファの情報を取得し，
 *          フレームバッファを設定する
 */
static void terminal_configure_framebuffer(const struct multiboot2_tag_framebuffer *fb)
{
	/* フレームバッファの情報が無効な場合は設定を行わない */
	if (!fb || fb->framebuffer_type != 1 || (fb->framebuffer_bpp != 32 && fb->framebuffer_bpp != 24))
	{
		return;
	}
	/* フレームバッファの解像度がVGAセルサイズに満たない場合は設定を行わない */
	if (fb->framebuffer_width < VGA_WIDTH * KFS_FB_CELL_WIDTH ||
		fb->framebuffer_height < VGA_HEIGHT * KFS_FB_CELL_HEIGHT)
	{
		return;
	}
	/* フレームバッファを物理メモリにマップする */
	if (framebuffer_map(fb->framebuffer_addr, fb->framebuffer_pitch, fb->framebuffer_height) < 0)
	{
		return;
	}

	kfs_framebuffer.pitch = fb->framebuffer_pitch;
	kfs_framebuffer.width = fb->framebuffer_width;
	kfs_framebuffer.height = fb->framebuffer_height;
	kfs_framebuffer.bpp = fb->framebuffer_bpp;
	kfs_framebuffer.red_pos = fb->red_field_position;
	kfs_framebuffer.red_size = fb->red_mask_size;
	kfs_framebuffer.green_pos = fb->green_field_position;
	kfs_framebuffer.green_size = fb->green_mask_size;
	kfs_framebuffer.blue_pos = fb->blue_field_position;
	kfs_framebuffer.blue_size = fb->blue_mask_size;
	kfs_framebuffer.enabled = 1;
}

/** Multiboot情報から端末の表示バックエンドを設定する
 * @param mbi_ptr Multiboot情報構造体の物理アドレス
 * @param magic ブートローダが渡したMultiboot magic値
 * @details Multiboot2のフレームバッファタグを探し，見つかった場合はフレームバッファを設定する
 * @note 主としてUEFI環境での設定であり，BIOS環境では何もしない．
 *       つまり，UEFIブートローダがMultiboot2を使用するため，
 *       UEFI環境でフレームバッファが提供されている場合に，
 *       この関数がフレームバッファを設定することになる．
 *       BIOSのブートローダはMultiboot1を使用することが多いため，
 *       BIOS環境ではこの関数は何もせずに戻ることになる．
 */
void terminal_configure_from_multiboot(unsigned long mbi_ptr, uint32_t magic)
{
	/* ブートローダが渡したマジック値が正しくない場合，
	 * またはMultiboot情報構造体のアドレスが無効な場合は設定を行わない．
	 * たとえばBIOSのブートローダはMultiboot1を使用することが多く，その場合はマジック値が異なるため，
	 * この関数は何もせずに戻ることになる．
	 */
	if (magic != MULTIBOOT2_BOOTLOADER_MAGIC || mbi_ptr == 0)
	{
		return;
	}

	const struct multiboot2_info *info =
		(struct multiboot2_info *)__va(mbi_ptr);				/* Multiboot情報構造体の仮想アドレス */
	unsigned long cursor = (unsigned long)info + sizeof(*info); /* 現在のタグ位置を示すカーソル */
	const unsigned long end = (unsigned long)info + info->total_size; /* Multiboot情報構造体の終了位置 */
	struct multiboot2_tag *tag;										  /* 現在のタグを指すポインタ */

	/* 各タグを順に処理する */
	while (cursor + sizeof(*tag) <= end)
	{
		tag = (struct multiboot2_tag *)cursor;

		/* タグの種類がENDの場合は処理を終了する */
		if (tag->type == MULTIBOOT2_TAG_TYPE_END)
		{
			break;
		}

		/* タグの種類がFRAMEBUFFERの場合はフレームバッファを設定する */
		if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER)
		{
			terminal_configure_framebuffer((const struct multiboot2_tag_framebuffer *)tag);
			return;
		}

		/* 次のタグへ移動する */
		cursor += (tag->size + 7) & ~7UL;
	}
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
	enum vga_color fg = (enum vga_color)(con->color & VGA_COLOR_INDEX_MASK);
	enum vga_color bg = (enum vga_color)((con->color >> VGA_COLOR_BACKGROUND_SHIFT) & VGA_COLOR_INDEX_MASK);
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
	/* VGAに書き込むバッファが存在しない場合，
	 * またはフレームバッファが有効でない場合は書き込まない */
	if (!kfs_terminal_buffer && !kfs_framebuffer.enabled)
	{
		return;
	}

	for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
	{
		terminal_write_hw_cell(con, i, con->shadow[i]);
	}
	framebuffer_flush_shadow(con);
}

/* VGAに書き込まれた値をシャドウバッファに書き込む */
static void console_capture_from_hw(struct kfs_console_state *con)
{
	if (kfs_framebuffer.enabled)
	{
		return;
	}
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
	framebuffer_sync_cursor(con);
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
		terminal_write_hw_cell(con, y * VGA_WIDTH + x, entry);
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
			terminal_write_hw_cell(con, y * VGA_WIDTH + i, con->shadow[y * VGA_WIDTH + i]);
		}
	}

	/* カーソル位置に新しい文字を挿入 */
	uint16_t entry = kfs_vga_make_entry(c, con->color);
	con->shadow[y * VGA_WIDTH + x] = entry;
	if (console_is_active(con) && kfs_terminal_buffer)
	{
		terminal_write_hw_cell(con, y * VGA_WIDTH + x, entry);
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
				terminal_write_hw_cell(con, (y - 1) * VGA_WIDTH + x, value);
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
			terminal_write_hw_cell(con, (VGA_HEIGHT - 1) * VGA_WIDTH + x, blank);
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
			terminal_write_hw_cell(con, y * VGA_WIDTH + i, con->shadow[y * VGA_WIDTH + i]);
		}
	}

	/* 行末を空白で埋める */
	uint16_t blank = kfs_vga_make_entry(' ', con->color);
	con->shadow[y * VGA_WIDTH + (VGA_WIDTH - 1)] = blank;
	if (console_is_active(con) && kfs_terminal_buffer)
	{
		terminal_write_hw_cell(con, y * VGA_WIDTH + (VGA_WIDTH - 1), blank);
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
			terminal_write_hw_cell(con, screen_line * VGA_WIDTH + x, con->scrollback[buf_line * VGA_WIDTH + x]);
		}
		screen_line++;
	}

	/* 残りは現在のshadowバッファから表示 */
	for (int i = 0; i < lines_from_shadow; i++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			terminal_write_hw_cell(con, screen_line * VGA_WIDTH + x, con->shadow[i * VGA_WIDTH + x]);
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
