#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/printk.h>

#include <kfs/printk.h> /* TODO: umbrella 削除後は個別ヘッダに整理 */
#include <stddef.h>
#include <stdint.h>

/* @see https://wiki.osdev.org/I8042_PS/2_Controller */
#define PS2_STATUS_PORT 0x64 /* PS/2 コントローラのペリフェラルから受け取るステータスレジスタのポート番号 */
#define PS2_DATA_PORT 0x60 /* PS/2 コントローラのデータポート番号 */

static int left_shift;
static int right_shift;
static int alt_pressed;
static int caps_lock;
static int extended_prefix; /* 拡張コードプレフィックス。これと、この次に押下された値によって、意味が決まる */
static int keyboard_initialized;

extern uint8_t kfs_io_inb(uint16_t port);

static const char scancode_map_normal[128] = {
	[0x02] = '1',  [0x03] = '2', [0x04] = '3',	[0x05] = '4', [0x06] = '5', [0x07] = '6',  [0x08] = '7',
	[0x09] = '8',  [0x0A] = '9', [0x0B] = '0',	[0x0C] = '-', [0x0D] = '=', [0x0F] = '\t', [0x10] = 'q',
	[0x11] = 'w',  [0x12] = 'e', [0x13] = 'r',	[0x14] = 't', [0x15] = 'y', [0x16] = 'u',  [0x17] = 'i',
	[0x18] = 'o',  [0x19] = 'p', [0x1A] = '[',	[0x1B] = ']', [0x1E] = 'a', [0x1F] = 's',  [0x20] = 'd',
	[0x21] = 'f',  [0x22] = 'g', [0x23] = 'h',	[0x24] = 'j', [0x25] = 'k', [0x26] = 'l',  [0x27] = ';',
	[0x28] = '\'', [0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',  [0x2F] = 'v',
	[0x30] = 'b',  [0x31] = 'n', [0x32] = 'm',	[0x33] = ',', [0x34] = '.', [0x35] = '/',  [0x39] = ' ',
};

static const char scancode_map_shift[128] = {
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
	[0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+', [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
	[0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
	[0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
	[0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C',
	[0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

/* シフトキーが押されているかどうかを判定する */
static int shift_active(void)
{
	return left_shift || right_shift;
}

/* スキャンコードから対応するascii codeを取得する */
static char translate_scancode(uint8_t code)
{
	char base = scancode_map_normal[code];
	if (!base)
		return 0;
	if (base >= 'a' && base <= 'z')
	{
		int upper = caps_lock ^ shift_active();
		return (char)(upper ? (base - ('a' - 'A')) : base);
	}
	if (shift_active())
	{
		char shifted = scancode_map_shift[code];
		if (shifted)
			return shifted;
	}
	return base;
}

static void handle_backspace(void)
{
	size_t row = 0;
	size_t col = 0;
	kfs_terminal_get_cursor(&row, &col);
	if (col > 0)
	{
		/* カーソルを1つ左に移動してから削除 */
		kfs_terminal_move_cursor(row, col - 1);
		terminal_delete_char(); /* 挿入モード対応: 文字を削除して左シフト */
	}
	else if (row > 0)
	{
		/* 前の行の末尾に移動してから削除 */
		size_t new_col = KFS_VGA_WIDTH - 1;
		kfs_terminal_move_cursor(row - 1, new_col);
		terminal_delete_char(); /* 挿入モード対応: 文字を削除して左シフト */
	}
}

/* キーボード状態を初期化 (または初期状態へ戻す) ためのヘルパー関数 */
void kfs_keyboard_reset(void)
{
	left_shift = 0;
	right_shift = 0;
	alt_pressed = 0;
	caps_lock = 0;
	extended_prefix = 0;
	keyboard_initialized = 0;
}

/* PS/2 キーボード制御の初期化ルーチン */
void kfs_keyboard_init(void)
{
	kfs_keyboard_reset();
	keyboard_initialized = 1;
	/* キーボードからの出力バッファ(CPUにとっては入力バッファ)
	 * の状態が1の間はポート 0x60 から 1バイト読んでやる処理
	 * @see https://wiki.osdev.org/I8042_PS/2_Controller
	 * まだ残っているキーボード入力を読み捨てる処理
	 */
	while (kfs_io_inb(PS2_STATUS_PORT) & 0x01)
	{
		/* FIFOなので1バイト読み捨てる */
		(void)kfs_io_inb(PS2_DATA_PORT);
	}
}

/* キーボードからの入力値に対応するasciiコードをVGAに出力する
 * @param scancode キーボードからの入力値(8bit)
 *  - 上位1ビット: キーの押下(0)または解放(1)を示すフラグ
 *  - 下位7ビット: 対応するキーコード
 * @see https://homepages.cwi.nl/~aeb/linux/kbd/scancodes-7.html#kscancodes
 */
void kfs_keyboard_feed_scancode(uint8_t scancode)
{
	/* 0xE0, 0xE1 は拡張コードのプレフィックス
	 * 拡張コードのプレフィックスは、それ単体では意味を持たないため、値を保持し処理を抜ける
	 * @see https://homepages.cwi.nl/~aeb/linux/kbd/scancodes-1.html
	 */
	if (scancode == 0xE0 || scancode == 0xE1)
	{
		extended_prefix = scancode;
		return;
	}

	int release = (scancode & 0x80) != 0; /* scancodeの上位1ビット。キーの押下(0)または解放(1)を示すフラグ */
	uint8_t code = scancode & 0x7F; /* scancodeの下位7ビット。対応するキーコード */

	/* 特殊キーの処理 */
	switch (code)
	{
	case 0x2A:
		left_shift = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x36:
		right_shift = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x38:
		alt_pressed = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x3A:
		if (!release)
			caps_lock = !caps_lock;
		extended_prefix = 0;
		return;
	case 0x0E:
		if (!release)
			handle_backspace();
		extended_prefix = 0;
		return;
	/* Enter */
	case 0x1C:
		if (!release)
			terminal_putchar('\n');
		extended_prefix = 0;
		return;
	/* F1 - F4 */
	case 0x3B:
	case 0x3C:
	case 0x3D:
	case 0x3E:
		if (!release && alt_pressed)
		{
			size_t target = (size_t)(code - 0x3B);
			if (target < kfs_terminal_console_count())
				kfs_terminal_switch_console(target);
		}
		extended_prefix = 0;
		return;
	default:
		break;
	}
	if (release)
	{
		extended_prefix = 0;
		return;
	}
	/* 拡張コード（0xE0）の後の特殊キー処理 */
	if (extended_prefix == 0xE0)
	{
		extended_prefix = 0;
		/* 矢印キーの処理 */
		if (code == 0x48) /* 上矢印 */
		{
			kfs_terminal_scroll_up();
			return;
		}
		else if (code == 0x50) /* 下矢印 */
		{
			kfs_terminal_scroll_down();
			return;
		}
		else if (code == 0x4B) /* 左矢印 */
		{
			kfs_terminal_cursor_left();
			return;
		}
		else if (code == 0x4D) /* 右矢印 */
		{
			kfs_terminal_cursor_right();
			return;
		}
		/* その他の拡張コードは無視 */
		return;
	}
	extended_prefix = 0;

	/* スキャンコードから対応するascii codeを取得しVGAに書き込む */
	char ch = translate_scancode(code);
	if (ch)
		terminal_putchar(ch);
}

/* キーボードのポーリング処理 (割り込み未使用時に定期的に呼び出す) */
void kfs_keyboard_poll(void)
{
	if (!keyboard_initialized)
		return;
	for (;;)
	{
		uint8_t status = kfs_io_inb(PS2_STATUS_PORT);
		if (!(status & 0x01)) /* キーボードからCPUへの出力バッファが空である(キーが押下されていない)ならば抜ける */
			break;
		uint8_t scancode = kfs_io_inb(PS2_DATA_PORT); /* キーボードから押下された値をscancodeに格納する */
		kfs_keyboard_feed_scancode(scancode);
	}
}
