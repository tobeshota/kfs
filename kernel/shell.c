#include <kfs/console.h>
#include <kfs/keyboard.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/shell.h>
#include <kfs/stdint.h>
#include <kfs/string.h>

#define SHELL_PROMPT "kfs $ " /* シェルプロンプト文字列 */
#define CMD_BUFFER_SIZE 256	  /* コマンドバッファのサイズ */
#define PS2_STATUS_PORT 0x64 /* PS/2 コントローラのペリフェラルから受け取るステータスレジスタのポート番号 */
#define PS2_RESET_COMMAND 0xFE /* PS/2 コントローラのリセットコマンド */

/* シェルの状態を保持する構造体 */
static struct
{
	char cmd_buffer[CMD_BUFFER_SIZE]; /* 入力されたコマンド文字列を格納 */
	size_t cmd_len;					  /* 現在のコマンド長 */
	size_t prompt_row;				  /* プロンプトが表示されている行 */
	size_t prompt_col;				  /* プロンプト終了後のカーソル位置（入力開始位置） */
	int initialized;				  /* 初期化済みフラグ */
} shell_state;

/* プロンプトを表示する。ユーザに入力待機状態を示すために必要 */
static void show_prompt(void)
{
	printk(SHELL_PROMPT);
	/* プロンプト表示後のカーソル位置を記録（入力領域の開始位置） */
	kfs_terminal_get_cursor(&shell_state.prompt_row, &shell_state.prompt_col);
}

/* コマンドバッファをクリアする。次のコマンド入力の準備をするために必要 */
static void clear_command_buffer(void)
{
	shell_state.cmd_len = 0;
	shell_state.cmd_buffer[0] = '\0';
}

/* システムを停止する（halt組み込みコマンド） */
static void cmd_halt(void)
{
	printk("System halted.\n");
	/* 割り込みを無効化してからCPUを完全に停止 */
	asm volatile("cli"); /* Clear Interrupt Flag - 割り込み無効化 */
	for (;;)
	{
		asm volatile("hlt"); /* CPUを停止状態にする */
	}
}

/* システムを再起動する（reboot組み込みコマンド） */
static void cmd_reboot(void)
{
	printk("Rebooting...\n");
	/* キーボードコントローラを使ってシステムをリセット
	 * 0x64ポート(PS/2ステータス)に0xFE(リセット)を送信すると、
	 * ほとんどの環境で即座にCPUリセットが発生し、次の行には到達しない */
	asm volatile("outb %b0, %w1" : : "a"((uint8_t)PS2_RESET_COMMAND), "Nd"((uint16_t)PS2_STATUS_PORT));

	/* ここには通常到達しない（リセットが即座に実行されるため）
	 * 古いハードウェアやリセット未対応の環境での安全策として待機 */
	for (;;)
	{
		asm volatile("hlt");
	}
} /* コマンドを実行する。入力された文字列を解析して対応する処理を行う */
static void execute_command(const char *cmd)
{
	/* 空コマンドは無視 */
	if (cmd[0] == '\0')
		return;

	/* halt コマンド */
	if (strcmp(cmd, "halt") == 0)
	{
		cmd_halt();
		return; /* この行には到達しないが、明示的に記載 */
	}

	/* reboot コマンド */
	if (strcmp(cmd, "reboot") == 0)
	{
		cmd_reboot();
		return; /* この行には到達しないが、明示的に記載 */
	}

	/* カーネルスタックダンプ */
	if (strcmp(cmd, "dkstack") == 0)
	{
		extern void dump_stack(void);
		dump_stack();
		return;
	}

	/* カーネルパニックテスト（KFS-3 Phase 2） */
	if (strcmp(cmd, "panic") == 0)
	{
		extern void panic(const char *fmt, ...);
		panic("Test panic from shell command");
		return; /* この行には到達しない */
	}

	/* メモリ情報表示 */
	if (strcmp(cmd, "meminfo") == 0)
	{
		extern void show_mem_info(void);
		show_mem_info();
		return;
	}

	/* TODO: 将来的にコマンドテーブルを使った実装に拡張 */
	printk("Unknown command: %s\n", cmd);
}

/** キーボードハンドラ：キーボードドライバから呼ばれる
 * @param c 入力された文字（通常文字、'\n', '\b', 制御文字など）
 * @return 処理した場合は1、処理しなかった場合は0
 */
int shell_keyboard_handler(char c)
{
	/* シェルが初期化されていない場合は処理しない（デフォルト動作に任せる） */
	if (!shell_state.initialized)
		return 0;

	/* 左矢印キー (0x1C) */
	if (c == '\x1C')
	{
		size_t row, col;
		kfs_terminal_get_cursor(&row, &col);

		/* プロンプト開始位置より左には移動させない */
		if (row < shell_state.prompt_row || (row == shell_state.prompt_row && col <= shell_state.prompt_col))
			return 1; /* 処理済み扱い */

		/* 通常のカーソル左移動 */
		kfs_terminal_cursor_left();
		return 1; /* 処理した */
	}

	/* 右矢印キー (0x1D) */
	if (c == '\x1D')
	{
		size_t row, col;
		kfs_terminal_get_cursor(&row, &col);

		/* 入力済み文字列の末尾より右には移動させない */
		size_t input_end_col = shell_state.prompt_col + shell_state.cmd_len;

		/* 同じ行で、かつ入力末尾より右には移動しない */
		if (row == shell_state.prompt_row && col >= input_end_col)
			return 1; /* 処理済み扱い */

		/* 通常のカーソル右移動 */
		kfs_terminal_cursor_right();
		return 1; /* 処理した */
	}

	/* 改行の場合はコマンドを実行 */
	if (c == '\n' || c == '\r')
	{
		printk("\n");
		/* NULL終端を確実にする */
		shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
		execute_command(shell_state.cmd_buffer);
		clear_command_buffer();
		show_prompt();
		return 1; /* 処理した */
	}

	/* バックスペースの処理 */
	if (c == '\b' || c == 127) /* 127はDELキー */
	{
		if (shell_state.cmd_len > 0)
		{
			/* 現在のカーソル位置を取得 */
			size_t row, col;
			kfs_terminal_get_cursor(&row, &col);

			/* プロンプト開始位置より左には移動させない */
			if (row < shell_state.prompt_row || (row == shell_state.prompt_row && col <= shell_state.prompt_col))
				return 1; /* 処理済み扱い */

			/* カーソル位置に対応するバッファ内のインデックスを計算 */
			size_t cursor_index = col - shell_state.prompt_col;

			/* カーソルより左の文字を削除 */
			if (cursor_index > 0 && cursor_index <= shell_state.cmd_len)
			{
				/* バッファ内で文字を左にシフト */
				for (size_t i = cursor_index - 1; i < shell_state.cmd_len - 1; i++)
				{
					shell_state.cmd_buffer[i] = shell_state.cmd_buffer[i + 1];
				}
				shell_state.cmd_len--;
				shell_state.cmd_buffer[shell_state.cmd_len] = '\0';

				/* 画面上でカーソルを左に移動 */
				kfs_terminal_move_cursor(row, col - 1);
				/* 右側の文字を左にシフト（terminal_delete_char使用） */
				terminal_delete_char();
			}
		}
		return 1; /* 処理した */
	}

	/* バッファオーバーフローを防ぐ */
	if (shell_state.cmd_len >= CMD_BUFFER_SIZE - 1)
	{
		printk("\nCommand too long!\n");
		clear_command_buffer();
		show_prompt();
		return 1; /* 処理した */
	}

	/* 制御文字は無視（タブなど将来拡張可能） */
	if (c < 32 && c != '\t')
		return 1; /* 処理した（無視） */

	/* 通常文字をバッファに追加して画面に表示 */
	shell_state.cmd_buffer[shell_state.cmd_len++] = c;
	printk("%c", c);
	return 1; /* 処理した */
}

/** プロンプト表示とキーボードハンドラ登録を行う
 *
 * @note 起動時に一度だけ呼ばれる
 */
void shell_init(void)
{
	if (shell_state.initialized)
		return;

	clear_command_buffer();
	shell_state.initialized = 1;

	/* キーボードハンドラを登録（依存性の注入） */
	kfs_keyboard_set_handler(shell_keyboard_handler);

	show_prompt();
}

/** シェルのメインループ
 * キーボード入力を受け付けてコマンドを処理する
 * テスト環境ではオーバーライドされる
 */
__attribute__((weak)) void shell_run(void)
{
	shell_init();

	/* キーボードとシリアル入力の無限ポーリングループ
	 * 割り込みが実装されるまではhltを使わずポーリングで処理 */
	for (;;)
	{
		/* キーボード入力をポーリング */
		kfs_keyboard_poll();

		/* シリアルポート入力をポーリング */
		int c = serial_read();
		if (c != -1)
		{
			/* シリアルからの入力を処理（キーボードハンドラを再利用） */
			shell_keyboard_handler((char)c);
		}
	}
}

/* 単体テストやドライバがシェルの初期化状態を問い合わせるためのヘルパ */
int shell_is_initialized(void)
{
	return shell_state.initialized;
}
