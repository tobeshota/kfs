#include <asm-i386/ptrace.h>
#include <kfs/console.h>
#include <kfs/irq.h>
#include <kfs/keyboard.h>
#include <kfs/printk.h>
#include <kfs/stddef.h>
#include <kfs/stdint.h>

/* @see https://wiki.osdev.org/I8042_PS/2_Controller */
#define PS2_STATUS_PORT 0x64 /* PS/2 コントローラのペリフェラルから受け取るステータスレジスタのポート番号 */
#define PS2_DATA_PORT 0x60 /* PS/2 コントローラのデータポート番号 */

static int left_shift;
static int right_shift;
static int alt_pressed;
static int caps_lock;

/** 拡張コードプレフィックス（0xE0, 0xE1）
 * これと、この次に押下された値によって、意味が決まる
 */
static int extended_prefix;

/* カスタムキーボードハンドラ（シェルなどが登録する） */
static keyboard_handler_t custom_handler = NULL;

/* 現在のキーボードレイアウト */
static kbd_layout_t current_layout = KBD_LAYOUT_QWERTY;

extern uint8_t kfs_io_inb(uint16_t port);

/* QWERTY配列（US）- 通常キー */
static const char scancode_map_qwerty_normal[128] = {
	[0x02] = '1',  [0x03] = '2', [0x04] = '3',	[0x05] = '4', [0x06] = '5', [0x07] = '6',  [0x08] = '7',
	[0x09] = '8',  [0x0A] = '9', [0x0B] = '0',	[0x0C] = '-', [0x0D] = '=', [0x0F] = '\t', [0x10] = 'q',
	[0x11] = 'w',  [0x12] = 'e', [0x13] = 'r',	[0x14] = 't', [0x15] = 'y', [0x16] = 'u',  [0x17] = 'i',
	[0x18] = 'o',  [0x19] = 'p', [0x1A] = '[',	[0x1B] = ']', [0x1E] = 'a', [0x1F] = 's',  [0x20] = 'd',
	[0x21] = 'f',  [0x22] = 'g', [0x23] = 'h',	[0x24] = 'j', [0x25] = 'k', [0x26] = 'l',  [0x27] = ';',
	[0x28] = '\'', [0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',  [0x2F] = 'v',
	[0x30] = 'b',  [0x31] = 'n', [0x32] = 'm',	[0x33] = ',', [0x34] = '.', [0x35] = '/',  [0x39] = ' ',
};

/* QWERTY配列（US）- Shiftキー押下時 */
static const char scancode_map_qwerty_shift[128] = {
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
	[0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+', [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
	[0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
	[0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
	[0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C',
	[0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

/* AZERTY配列（フランス語）- 通常キー */
static const char scancode_map_azerty_normal[128] = {
	[0x02] = '&', [0x03] = 'e', [0x04] = '"', [0x05] = '\'', [0x06] = '(', [0x07] = '-',  [0x08] = 'e',
	[0x09] = '_', [0x0A] = 'c', [0x0B] = 'a', [0x0C] = ')',	 [0x0D] = '=', [0x0F] = '\t', [0x10] = 'a',
	[0x11] = 'z', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',	 [0x15] = 'y', [0x16] = 'u',  [0x17] = 'i',
	[0x18] = 'o', [0x19] = 'p', [0x1A] = '^', [0x1B] = '$',	 [0x1E] = 'q', [0x1F] = 's',  [0x20] = 'd',
	[0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',	 [0x25] = 'k', [0x26] = 'l',  [0x27] = 'm',
	[0x28] = 'u', [0x29] = '*', [0x2B] = '*', [0x2C] = 'w',	 [0x2D] = 'x', [0x2E] = 'c',  [0x2F] = 'v',
	[0x30] = 'b', [0x31] = 'n', [0x32] = ',', [0x33] = ';',	 [0x34] = ':', [0x35] = '!',  [0x39] = ' ',
};

/* AZERTY配列（フランス語）- Shiftキー押下時 */
static const char scancode_map_azerty_shift[128] = {
	[0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
	[0x0A] = '9', [0x0B] = '0', [0x0C] = 'o', [0x0D] = '+', [0x10] = 'A', [0x11] = 'Z', [0x12] = 'E', [0x13] = 'R',
	[0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', [0x1A] = '"', [0x1B] = 'L',
	[0x1E] = 'Q', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
	[0x26] = 'L', [0x27] = 'M', [0x28] = '%', [0x29] = 'u', [0x2B] = 'u', [0x2C] = 'W', [0x2D] = 'X', [0x2E] = 'C',
	[0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = '?', [0x33] = '.', [0x34] = '/', [0x35] = 'S',
};

/* シフトキーが押されているかどうかを判定する */
static int shift_active(void)
{
	return left_shift || right_shift;
}

/* スキャンコードから対応するascii codeを取得する */
static char translate_scancode(uint8_t code)
{
	/* 現在のレイアウトに対応するキーマップを選択 */
	const char *map_normal;
	const char *map_shift;

	if (current_layout == KBD_LAYOUT_AZERTY)
	{
		map_normal = scancode_map_azerty_normal;
		map_shift = scancode_map_azerty_shift;
	}
	else
	{
		map_normal = scancode_map_qwerty_normal;
		map_shift = scancode_map_qwerty_shift;
	}

	char base = map_normal[code];
	if (!base)
	{
		return 0;
	}
	if (base >= 'a' && base <= 'z')
	{
		int upper = caps_lock ^ shift_active();
		return (char)(upper ? (base - ('a' - 'A')) : base);
	}
	if (shift_active())
	{
		char shifted = map_shift[code];
		if (shifted)
		{
			return shifted;
		}
	}
	return base;
}

/* バックスペース処理：カーソルを左に移動して文字を削除 */
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
		kfs_terminal_move_cursor(row - 1, KFS_VGA_WIDTH - 1);
		terminal_delete_char(); /* 挿入モード対応: 文字を削除して左シフト */
	}
}

/* キーボード状態をリセットする */
void kfs_keyboard_reset(void)
{
	left_shift = 0;
	right_shift = 0;
	alt_pressed = 0;
	caps_lock = 0;
	extended_prefix = 0;
	custom_handler = NULL;
	current_layout = KBD_LAYOUT_QWERTY;
}

/** キーボードレイアウトを設定する
 * @param layout 設定するレイアウト(KBD_LAYOUT_QWERTY または KBD_LAYOUT_AZERTY)
 */
void kfs_keyboard_set_layout(kbd_layout_t layout)
{
	current_layout = layout;
}

/** 現在のキーボードレイアウトを取得する
 * @return 現在のレイアウト
 */
kbd_layout_t kfs_keyboard_get_layout(void)
{
	return current_layout;
}

/** キーボードIRQハンドラ（IRQ1から呼ばれる）
 * @param irq   IRQ番号（1）
 * @param regs  割り込み時のレジスタ状態
 * @return 常に0（成功）
 */
static int keyboard_interrupt(int irq, struct pt_regs *regs)
{
	(void)irq;
	(void)regs;

	/* キーボードデータを読み取り、スキャンコードを処理 */
	uint8_t scancode = kfs_io_inb(PS2_DATA_PORT);
	kfs_keyboard_feed_scancode(scancode);

	return 0;
}

/** PS/2キーボードドライバを初期化する
 * @note 割り込みを用いるためIDT/PIC初期化後に呼び出すこと
 */
void kfs_keyboard_init(void)
{
	kfs_keyboard_reset();

	/** キーボードからの出力バッファ(CPUにとっては入力バッファ)を読み捨てる
	 * @see https://wiki.osdev.org/I8042_PS/2_Controller
	 */
	while (kfs_io_inb(PS2_STATUS_PORT) & 0x01)
	{
		(void)kfs_io_inb(PS2_DATA_PORT);
	}

	/** IRQ1にキーボードハンドラを登録する
	 * @details
	 * IRQ番号(KEYBOARD_IRQ番)とISRアドレス(keyboard_interrupt)を対応づける．
	 * これにより，KEYBOARD_IRQ番に割り込みを起こした時，keyboard_interruptが呼ばれるようになる．
	 *
	 * 【図解】割り込み発生からISR呼び出しまでの流れ:
	 * キー押下 -> PS/2コントローラ: IRQ番号(KEYBOARD_IRQ番)
	 *         -> 8259A: IRQ番号(KEYBOARD_IRQ番)
	 *         -> CPU: INTA
	 *         -> 8259A: Interrupt Vector番号(0x21)
	 *         -> CPU: ISRアドレス(irq1)
	 * 	         -> do_IRQ()
	 *           -> keyboard_interrupt()
	 *
	 * 【詳説】割り込み発生からISR呼び出しまでの流れ:
	 * 1. IRQ番号 と Interrupt Vector番号 と ISRアドレス を対応づける
	 *    (KEYBOARD_IRQ番 と 0x21 と irq1 を対応づける)
	 *   a. main() -> init_8259A() より，
	 *      IRQ番号(KEYBOARD_IRQ番) から Interrupt Vector番号(0x21) を
	 *      8259A PIC(Programmable interrupt controller) が引けるようになる
	 *   b. main() -> idt_init()
	 *             -> init_IRQ()
	 *             -> set_intr_gate(0x21, irq1) より，
	 *      Interrupt Vector番号(0x21) から ISRアドレス(irq1) を
	 *      CPU が引けるようになる
	 *   c. main() -> kfs_keyboard_init()
	 *             -> request_irq(KEYBOARD_IRQ, keyboard_interrupt, "keyboard", NULL) より，
	 *      IRQ番号(KEYBOARD_IRQ番) から ISRアドレス(keyboard_interrupt) を
	 *      CPU(irq_actions配列) が引けるようになる
	 * 2. 割り込みを発生させる
	 *   a. 人間がキーを押す
	 *   b. PS/2コントローラ が IRQ番号(KEYBOARD_IRQ番) の割り込みを 8259A に 送信する
	 *     (PS/2コントローラ が IRQラインKEYBOARD_IRQ番 の電圧をLOWにするよう要求する
	 * 3. IRQ番号 から Interrupt Vector番号 を経て対応する ISR を呼び出す
	 *   a. 8259A が 受信した割り込み要求をCPUに送信する
	 *   b. CPUが 8259A からの割り込み要求を受け取り，これが有効化されているとき，
	 *      2回の INTA (Interrupt Acknowledge) サイクルを発行する
	 *   c. 8259Aが
	 *     - 1回目のサイクルで INTA を受け取り，割り込みの開始を認識する
	 *     - 2回目のサイクルで IRQ番号(KEYBOARD_IRQ番) から Interrupt Vector番号(0x21) を引き出し，
	 *       CPUに送信する
	 *   d. CPUが受信した Interrupt Vector番号(0x21) から ISRアドレス(irq1) を引き出し，呼び出す．
	 *      irq1 (entry.S) は do_IRQ() を呼び出し，
	 *      do_IRQ() は登録されたハンドラ keyboard_interrupt() を呼び出す
	 */
	if (request_irq(KEYBOARD_IRQ, keyboard_interrupt, "keyboard", NULL) < 0)
	{
		printk("keyboard: failed to register IRQ1\n");
		return;
	}

	/** CPUの割り込みを有効化
	 * @note 割り込み無効化はarch/i386/boot/boot.S: _start で行われている
	 */
	__asm__ __volatile__("sti");
}

/** キーボードからの入力値に対応するasciiコードをVGAに出力する
 * @param scancode キーボードからの入力値(8bit)
 *  - 上位1ビット: キーの押下(0)または解放(1)を示すフラグ
 *  - 下位7ビット: 対応するキーコード
 * @see https://homepages.cwi.nl/~aeb/linux/kbd/scancodes-7.html#kscancodes
 */
void kfs_keyboard_feed_scancode(uint8_t scancode)
{
	/** 0xE0, 0xE1 は拡張コードのプレフィックス
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
		{
			caps_lock = !caps_lock;
		}
		extended_prefix = 0;
		return;
	case 0x0E:
		if (!release)
		{
			/* バックスペース: ハンドラに渡す */
			if (custom_handler && custom_handler('\b'))
			{
				/* ハンドラが処理した */
			}
			else
			{
				/* ハンドラがないか、ハンドラが0を返した */
				handle_backspace();
			}
		}
		extended_prefix = 0;
		return;
	/* Enter */
	case 0x1C:
		if (!release)
		{
			if (custom_handler && custom_handler('\n'))
			{
				/* ハンドラが処理した */
			}
			else
			{
				/* 従来のEnter: 改行してカーソルを次行先頭へ */
				printk("\n");
			}
		}
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
			{
				kfs_terminal_switch_console(target);
			}
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
			/* ハンドラがあれば特殊コード（例: '\x1B'=ESC + 'D'）として渡す
			 * ここでは簡易的に制御文字 0x1C（左）を使う
			 */
			if (custom_handler && custom_handler('\x1C'))
			{
				/* ハンドラが処理した */
			}
			else
			{
				kfs_terminal_cursor_left(); /* 従来のカーソル移動 */
			}
			return;
		}
		else if (code == 0x4D) /* 右矢印 */
		{
			/* 同様に制御文字 0x1D（右）を使う */
			if (custom_handler && custom_handler('\x1D'))
			{
				/* ハンドラが処理した */
			}
			else
			{
				kfs_terminal_cursor_right();
			}
			return;
		}
		/* その他の拡張コードは無視 */
		return;
	}
	extended_prefix = 0;

	/* スキャンコードから対応するascii codeを取得
	 * - ハンドラがないか、ハンドラが0を返したら従来通り端末へ直接出力する
	 */
	char ch = translate_scancode(code);
	if (ch)
	{
		if (custom_handler && custom_handler(ch))
		{
			/* ハンドラが処理した */
		}
		else
		{
			printk("%c", ch);
		}
	}
}

/** カスタムキーボードハンドラを設定する
 * @param handler ハンドラ関数ポインタ（NULLでデフォルト動作に戻す）
 */
void kfs_keyboard_set_handler(keyboard_handler_t handler)
{
	custom_handler = handler;
}
