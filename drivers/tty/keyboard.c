#include <asm-i386/ptrace.h>
#include <kfs/console.h>
#include <kfs/irq.h>
#include <kfs/keyboard.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/signal.h>
#include <kfs/stddef.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <kfs/tty.h>

/* @see https://wiki.osdev.org/I8042_PS/2_Controller */
#define PS2_STATUS_PORT 0x64 /* PS/2 コントローラのペリフェラルから受け取るステータスレジスタのポート番号 */
#define PS2_DATA_PORT 0x60 /* PS/2 コントローラのデータポート番号 */
#define PS2_STATUS_OBF 0x01 /* Status Register: Output Buffer Full ビット。1 のとき DATA_PORT にデータあり */
#define SCANCODE_RELEASE_BIT 0x80 /* スキャンコード上位1ビット: 0=押下, 1=解放 */
#define SCANCODE_KEY_MASK 0x7F	  /* スキャンコード下位7ビット: キーコード本体 */

static int left_shift;
static int right_shift;
static int ctrl_pressed;
static int alt_pressed;
static int caps_lock;

/** 拡張コードプレフィックス（0xE0, 0xE1）
 * これと、この次に押下された値によって、意味が決まる
 */
static int extended_prefix;

/** カスタムキーボードハンドラ（シェルなどが登録する）
 * @note レイアウト変換済み ASCII 文字・押下時のみ通知。
 *       シェルのように「文字」として扱いたい用途に使う。
 */
static keyboard_handler_t custom_handler = NULL;

/** RAW スキャンコードハンドラ（piano モードなどが登録する）
 * @note 物理スキャンコード・押下と解放の両方を通知。
 *       piano のように「どの物理キーが今押されているか」を
 *       追跡する必要がある用途に使う。
 */
static keyboard_raw_handler_t raw_handler = NULL;

/* 現在のキーボードレイアウト */
static kbd_layout_t current_layout = KBD_LAYOUT_QWERTY;

#define KEYBOARD_RAW_QUEUE_SIZE 64
static struct kfs_keyboard_raw_event keyboard_raw_queue[KEYBOARD_RAW_QUEUE_SIZE]; /* RAWイベントのリングバッファ */
static unsigned int keyboard_raw_head;			/* RAWイベントキューの先頭インデックス */
static unsigned int keyboard_raw_tail;			/* RAWイベントキューの末尾インデックス */
static unsigned int keyboard_raw_count;			/* RAWイベントキュー内のイベント数 */
static struct task_struct *keyboard_raw_waiter; /* RAWイベント待ちのタスク */
static int keyboard_raw_mode; /* RAWモードフラグ。1のときraw_handlerにイベントを送る */

extern uint8_t kfs_io_inb(uint16_t port);

/* QWERTY配列（US）- 通常キー */
static const char scancode_map_qwerty_normal[KEYBOARD_SCANCODE_MAX] = {
	[0x02] = '1',  [0x03] = '2', [0x04] = '3',	[0x05] = '4', [0x06] = '5', [0x07] = '6',  [0x08] = '7',
	[0x09] = '8',  [0x0A] = '9', [0x0B] = '0',	[0x0C] = '-', [0x0D] = '=', [0x0F] = '\t', [0x10] = 'q',
	[0x11] = 'w',  [0x12] = 'e', [0x13] = 'r',	[0x14] = 't', [0x15] = 'y', [0x16] = 'u',  [0x17] = 'i',
	[0x18] = 'o',  [0x19] = 'p', [0x1A] = '[',	[0x1B] = ']', [0x1E] = 'a', [0x1F] = 's',  [0x20] = 'd',
	[0x21] = 'f',  [0x22] = 'g', [0x23] = 'h',	[0x24] = 'j', [0x25] = 'k', [0x26] = 'l',  [0x27] = ';',
	[0x28] = '\'', [0x29] = '`', [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',  [0x2F] = 'v',
	[0x30] = 'b',  [0x31] = 'n', [0x32] = 'm',	[0x33] = ',', [0x34] = '.', [0x35] = '/',  [0x39] = ' ',
};

/* QWERTY配列（US）- Shiftキー押下時 */
static const char scancode_map_qwerty_shift[KEYBOARD_SCANCODE_MAX] = {
	[0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
	[0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+', [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
	[0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
	[0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
	[0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~', [0x2B] = '|', [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C',
	[0x2F] = 'V', [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
};

/* AZERTY配列（フランス語）- 通常キー */
static const char scancode_map_azerty_normal[KEYBOARD_SCANCODE_MAX] = {
	[0x02] = '&', [0x03] = 'e', [0x04] = '"', [0x05] = '\'', [0x06] = '(', [0x07] = '-',  [0x08] = 'e',
	[0x09] = '_', [0x0A] = 'c', [0x0B] = 'a', [0x0C] = ')',	 [0x0D] = '=', [0x0F] = '\t', [0x10] = 'a',
	[0x11] = 'z', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',	 [0x15] = 'y', [0x16] = 'u',  [0x17] = 'i',
	[0x18] = 'o', [0x19] = 'p', [0x1A] = '^', [0x1B] = '$',	 [0x1E] = 'q', [0x1F] = 's',  [0x20] = 'd',
	[0x21] = 'f', [0x22] = 'g', [0x23] = 'h', [0x24] = 'j',	 [0x25] = 'k', [0x26] = 'l',  [0x27] = 'm',
	[0x28] = 'u', [0x29] = '*', [0x2B] = '*', [0x2C] = 'w',	 [0x2D] = 'x', [0x2E] = 'c',  [0x2F] = 'v',
	[0x30] = 'b', [0x31] = 'n', [0x32] = ',', [0x33] = ';',	 [0x34] = ':', [0x35] = '!',  [0x39] = ' ',
};

/* AZERTY配列（フランス語）- Shiftキー押下時 */
static const char scancode_map_azerty_shift[KEYBOARD_SCANCODE_MAX] = {
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

static void keyboard_publish_raw_event(uint8_t code, int release)
{
	if (keyboard_raw_count < KEYBOARD_RAW_QUEUE_SIZE)
	{
		keyboard_raw_queue[keyboard_raw_tail].code = code;
		keyboard_raw_queue[keyboard_raw_tail].release = (uint8_t)(release ? 1 : 0);
		keyboard_raw_tail = (keyboard_raw_tail + 1) % KEYBOARD_RAW_QUEUE_SIZE;
		keyboard_raw_count++;
	}

	if (keyboard_raw_waiter)
	{
		wake_up_process(keyboard_raw_waiter);
		keyboard_raw_waiter = NULL;
	}
}

long kfs_keyboard_read_line(char *buf, unsigned int size)
{
	return tty_read_line_for_console(current->tty_console, buf, size);
}

long kfs_keyboard_read_line_for_console(size_t console_index, char *buf, unsigned int size)
{
	return tty_read_line_for_console(console_index, buf, size);
}

static long keyboard_read_raw_event(struct kfs_keyboard_raw_event *event)
{
	if (!event)
	{
		return -1;
	}

	while (1)
	{
		__asm__ volatile("cli");
		if (keyboard_raw_count > 0)
		{
			*event = keyboard_raw_queue[keyboard_raw_head];
			keyboard_raw_head = (keyboard_raw_head + 1) % KEYBOARD_RAW_QUEUE_SIZE;
			keyboard_raw_count--;
			__asm__ volatile("sti");
			return 1;
		}

		keyboard_raw_waiter = current;
		current->__state = TASK_INTERRUPTIBLE;
		__asm__ volatile("sti");
		schedule();
	}
}

long kfs_keyboard_read_event(struct kfs_keyboard_raw_event *event)
{
	return keyboard_read_raw_event(event);
}

void kfs_keyboard_clear_events(void)
{
	__asm__ volatile("cli");
	keyboard_raw_head = 0;
	keyboard_raw_tail = 0;
	keyboard_raw_count = 0;
	__asm__ volatile("sti");
}

void kfs_keyboard_set_raw_mode(int enabled)
{
	keyboard_raw_mode = enabled ? 1 : 0;
	if (!keyboard_raw_mode)
	{
		kfs_keyboard_clear_events();
	}
}

/* Ctrl 押下時の文字変換: 文字キーを制御文字に落とす */
static char translate_ctrl_char(char ch)
{
	if (ch >= 'a' && ch <= 'z')
	{
		return (char)(ch - 'a' + 1);
	}
	if (ch >= 'A' && ch <= 'Z')
	{
		return (char)(ch - 'A' + 1);
	}
	return 0;
}

/* キーボード状態をリセットする */
void kfs_keyboard_reset(void)
{
	left_shift = 0;
	right_shift = 0;
	ctrl_pressed = 0;
	alt_pressed = 0;
	caps_lock = 0;
	extended_prefix = 0;
	custom_handler = NULL;
	raw_handler = NULL;
	current_layout = KBD_LAYOUT_QWERTY;
	tty_reset();
	keyboard_raw_head = 0;
	keyboard_raw_tail = 0;
	keyboard_raw_count = 0;
	keyboard_raw_waiter = NULL;
	keyboard_raw_mode = 0;
}

/** RAW スキャンコードハンドラを登録する
 * @param handler press/release 両方を受け取るハンドラ。NULL で解除。
 */
void kfs_keyboard_set_raw_handler(keyboard_raw_handler_t handler)
{
	raw_handler = handler;
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
	while (kfs_io_inb(PS2_STATUS_PORT) & PS2_STATUS_OBF)
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

	int release = (scancode & SCANCODE_RELEASE_BIT) != 0; /* 上位1ビットが1なら解放イベント */
	uint8_t code = scancode & SCANCODE_KEY_MASK;		  /* 下位7ビットがキーコード本体 */
	if (keyboard_raw_mode)
	{
		keyboard_publish_raw_event(code, release);
	}

	/* RAW ハンドラが登録されていれば先に呼ぶ (piano モードなど press/release 両方が必要な場合) */
	if (raw_handler && raw_handler(code, release))
	{
		extended_prefix = 0;
		return;
	}

	/* 特殊キーの処理 */
	switch (code)
	{
	case 0x2A: /* 左Shift */
		left_shift = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x36: /* 右Shift */
		right_shift = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x38: /* Alt */
		alt_pressed = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x1D: /* Ctrl */
		ctrl_pressed = release ? 0 : 1;
		extended_prefix = 0;
		return;
	case 0x3A: /* Caps Lock */
		if (!release)
		{
			caps_lock = !caps_lock;
		}
		extended_prefix = 0;
		return;
	case 0x0E: /* バックスペース */
		if (!release)
		{
			if (custom_handler)
			{
				/* 互換性のため custom handler 経路では従来どおりイベントを渡す */
				(void)custom_handler('\b');
			}
			else
			{
				tty_handle_backspace_for_console(kfs_terminal_active_console());
			}
		}
		extended_prefix = 0;
		return;

	case 0x1C: /* Enter */
		if (!release)
		{
			if (custom_handler && custom_handler('\n'))
			{
				/* ハンドラが処理した */
			}
			else
			{
				tty_handle_enter_for_console(kfs_terminal_active_console());
			}
		}
		extended_prefix = 0;
		return;
	/* F1 - F4 */
	case 0x3B: /* F1 */
	case 0x3C: /* F2 */
	case 0x3D: /* F3 */
	case 0x3E: /* F4 */
		if (!release && alt_pressed)
		{
			/* Alt + F1 - F4 で仮想コンソールを切り替える */
			size_t target = (size_t)(code - 0x3B); /* 0x3B = F1 */

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

	if (keyboard_raw_mode)
	{
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
			if (custom_handler)
			{
				/* 互換経路 */
				(void)custom_handler('\x1C');
			}
			else
			{
				tty_handle_cursor_left_for_console(kfs_terminal_active_console());
			}
			return;
		}
		else if (code == 0x4D) /* 右矢印 */
		{
			if (custom_handler)
			{
				/* 互換経路 */
				(void)custom_handler('\x1D');
			}
			else
			{
				tty_handle_cursor_right_for_console(kfs_terminal_active_console());
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
		if (ctrl_pressed)
		{
			char ctrl_char = translate_ctrl_char(ch);
			if (ctrl_char)
			{
				/* 端末がCtrl-Cを受け取ったとき，
				 * フォアグラウンドプロセスグループに属するプロセスに対して，
				 * SIGINTを送信する */
				if (ctrl_char == 0x03)
				{
					pid_t fgprg = kfs_terminal_get_foreground_pgrp_for_console(kfs_terminal_active_console());
					if (fgprg == 0)
					{
						fgprg = current->pgrp;
					}
					if (fgprg > 0)
					{
						(void)kill_pg(fgprg, SIGINT);
					}

					/* シェルが read(0, ...) で入力待ち中なら、^C を表示して
					 * 現在の入力行を破棄し、空行を publish して即座にプロンプトへ戻す。 */
					if (!custom_handler)
					{
						printk("^C\n");
						tty_discard_input_for_console(kfs_terminal_active_console(), 1);
					}

					extended_prefix = 0;
					return;
				}

				/* 端末がCtrl-Zを受け取ったとき，
				 * フォアグラウンドプロセスグループに属するプロセスに対して，
				 * SIGTSTPを送信する */
				if (ctrl_char == 0x1A)
				{
					pid_t fgprg = kfs_terminal_get_foreground_pgrp_for_console(kfs_terminal_active_console());
					if (fgprg == 0)
					{
						/* フォアグラウンドプロセスグループが設定されていない場合は
						 * 現在のプロセスのグループを使用する */
						fgprg = current->pgrp;
					}
					if (fgprg > 0)
					{
						(void)kill_pg(fgprg, SIGTSTP);
					}

					if (!custom_handler)
					{
						printk("^Z\n");
						tty_discard_input_for_console(kfs_terminal_active_console(), 1);
					}

					extended_prefix = 0;
					return;
				}
				ch = ctrl_char;
			}
		}
		if (custom_handler && custom_handler(ch))
		{
			/* ハンドラが処理した */
		}
		else
		{
			tty_input_char_for_console(kfs_terminal_active_console(), ch);
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
