#include <asm-i386/pgtable.h>
#include <kfs/console.h>
#include <kfs/exec.h>
#include <kfs/keyboard.h>
#include <kfs/neofetch.h>
#include <kfs/panic.h>
#include <kfs/prctl.h>
#include <kfs/printk.h>
#include <kfs/serial.h>
#include <kfs/shell.h>
#include <kfs/signal.h>
#include <kfs/stdint.h>
#include <kfs/string.h>
#include <kfs/sys.h>
#include <kfs/timer.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

#define SHELL_PROMPT "kfs $ " /* シェルプロンプト文字列 */
#define CMD_BUFFER_SIZE 256	  /* コマンドバッファのサイズ */
#define PS2_STATUS_PORT 0x64 /* PS/2 コントローラのペリフェラルから受け取るステータスレジスタのポート番号 */
#define PS2_RESET_COMMAND 0xFE /* PS/2 コントローラのリセットコマンド */

/* シェルの状態を保持する構造体 */
static struct
{
	char cmd_buffer[CMD_BUFFER_SIZE];  /* 入力されたコマンド文字列を格納 */
	size_t cmd_len;					   /* 現在のコマンド長 */
	size_t prompt_row;				   /* プロンプトが表示されている行 */
	size_t prompt_col;				   /* プロンプト終了後のカーソル位置（入力開始位置） */
	int initialized;				   /* 初期化済みフラグ */
	int cmd_ready;					   /* コマンド実行待ちフラグ（IRQ外で処理するため） */
	char pending_cmd[CMD_BUFFER_SIZE]; /* 実行待ちコマンド文字列 */
} shell_state;

static const char *shell_ring3_job_name;
static void (*shell_ring3_job_fn)(void);

static void shell_ring3_job_entry(void)
{
	prctl(PR_SET_NAME, (unsigned long)shell_ring3_job_name, 0, 0, 0);
	shell_ring3_job_fn();
	exit(0);
}

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

/** 入力行の末尾にある空白と `&` を解釈して正規化する。
 * @param input 元の入力行
 * @param output 正規化後のコマンド列
 * @param output_size output のサイズ
 * @param foreground 1=foreground, 0=background
 * @example
 * inputが "  cmd arg1 arg2  " の場合，
 * output に "cmd arg1 arg2" をセットし，
 * foreground には 1 をセットする。
 * @example
 * inputが "  cmd arg1 arg2 &  " の場合，
 * output に "cmd arg1 arg2" をセットし，
 * foreground には 0 をセットする。
 */
static void shell_parse_command_line(const char *input, char *output, size_t output_size, int *foreground)
{
	size_t len;

	*foreground = 1;
	len = strlen(input);
	while (len > 0 && input[len - 1] == ' ')
	{
		len--;
	}
	if (len > 0 && input[len - 1] == '&')
	{
		*foreground = 0;
		len--;
		while (len > 0 && input[len - 1] == ' ')
		{
			len--;
		}
	}
	if (len >= output_size)
	{
		len = output_size - 1;
	}
	memcpy(output, input, len);
	output[len] = '\0';
}

/** ring-3 プロセスを起動するヘルパー関数
 * foreground の場合は終了待ちし、終わったら shell に foreground を戻す。
 */
pid_t shell_launch_ring3_job(const char *name, void (*fn)(void), int foreground)
{
	shell_ring3_job_name = name;
	shell_ring3_job_fn = fn;
	pid_t child = do_fork((unsigned long)shell_ring3_job_entry);
	if (child < 0)
	{
		return child;
	}

	/* 親が子をプロセスグループリーダーにする */
	(void)sys_setpgid(child, child);
	if (foreground)
	{
		/* 端末のフォアグラウンドプロセスグループを子プロセスグループに移す */
		(void)sys_tcsetpgrp(0, child);
		do_waitpid(child, NULL, 0);
		/* 終了後は shell に foreground を戻す */
		(void)sys_tcsetpgrp(0, current->pgrp);
	}
	return child;
}

/* コマンドを実行する。入力された文字列を解析して対応する処理を行う */
static void execute_command(const char *line)
{
	char normalized_cmd[CMD_BUFFER_SIZE];
	int foreground;

	shell_parse_command_line(line, normalized_cmd, sizeof(normalized_cmd), &foreground);
	const char *cmd = normalized_cmd;

	if (cmd[0] == '\0')
	{
		return;
	}

	shell_builtin_fn fn;
	const char *args;
	if (lookup_builtin(cmd, &fn, &args))
	{
		fn(args, foreground);
		return;
	}

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
	{
		return 0;
	}

	/* 左矢印キー (0x1C) */
	if (c == '\x1C')
	{
		size_t row, col;
		kfs_terminal_get_cursor(&row, &col);

		/* プロンプト開始位置より左には移動させない */
		if (row < shell_state.prompt_row || (row == shell_state.prompt_row && col <= shell_state.prompt_col))
		{
			return 1; /* 処理済み扱い */
		}

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
		{
			return 1; /* 処理済み扱い */
		}

		/* 通常のカーソル右移動 */
		kfs_terminal_cursor_right();
		return 1; /* 処理した */
	}

	/* 改行の場合はコマンド実行フラグを立てる。
	 * keyboard IRQ コンテキスト外で execute_command を呼ぶことで、
	 * beep/sleep など do_fork + do_wait を使うコマンドが
	 * IRQ ハンドラ内でブロックして EOI が送れなくなる問題を防ぐ。 */
	if (c == '\n' || c == '\r')
	{
		printk("\n");
		shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
		/* pending_cmd にコピーしてフラグを立てる */
		for (size_t i = 0; i <= shell_state.cmd_len; i++)
		{
			shell_state.pending_cmd[i] = shell_state.cmd_buffer[i];
		}
		clear_command_buffer();
		shell_state.cmd_ready = 1;
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
			{
				return 1; /* 処理済み扱い */
			}

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

	/* Ctrl+C: 端末の foreground pgrp に SIGINT を送る (簡易実装) */
	if (c == '\x03')
	{
		pid_t fg = foreground_pgrp;
		if (fg == 0)
		{
			fg = current->pgrp;
		}
		kill_pg(fg, SIGINT);
		return 1;
	}

	/* '\x03'以外の制御文字は無視（タブなど将来拡張可能） */
	if (c < 32 && c != '\t')
	{
		return 1; /* 処理した（無視） */
	}

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
	{
		return;
	}

	clear_command_buffer();
	shell_state.initialized = 1;
	shell_builtins_init();

	/* キーボードハンドラを登録（依存性の注入） */
	kfs_keyboard_set_handler(shell_keyboard_handler);

	/* neofetch風のシステム情報画面を表示する */
	print_neofetch();

	show_prompt();
}

/** シェルのメインループ
 * キーボード入力を受け付けてコマンドを処理する
 *
 * この関数はweak symbolとして定義されており、
 * テスト環境では別の実装でオーバーライドできる
 */
__attribute__((weak)) void shell_run(void)
{
	shell_init();

	/* メインループ: 割り込みでキーボード入力を処理 */
	while (1)
	{
		/** シリアルポート入力を確認
		 * @note シリアルI/Oはデバッグ用途のためIRQラインではなくポーリング方式を用いる
		 */
		int c = serial_read();
		if (c != -1)
		{
			/* シリアルからの入力を処理（キーボードハンドラを再利用） */
			shell_keyboard_handler((char)c);
		}

		/*
		 * keyboard IRQ コンテキストの外でコマンドを実行する。
		 * shell_keyboard_handler が '\n' を受け取ると cmd_ready = 1 にして
		 * すぐに return する（IRQ ハンドラを解放して EOI を送信させる）。
		 * do_fork + do_wait を使うコマンドは
		 * IRQ コンテキストでブロックすると次の keyboard IRQ が届かなくなるため、
		 * schedule() で一度 CPU を譲ってからこのメインループで実行する。
		 */
		if (shell_state.cmd_ready)
		{
			shell_state.cmd_ready = 0;
			execute_command(shell_state.pending_cmd);
			show_prompt();
		}

		/* シェルの子プロセスがゾンビとして残らないよう、
		 * 定期的に非ブロッキングで回収する */
		while (1)
		{
			pid_t r = do_wait(NULL, WNOHANG);
			if (r <= 0)
			{
				break;
			}
		}

		/* CPU を他タスクへ譲る（hlt は cpu_idle_loop() で行う） */
		schedule();
	}
}

/* 単体テストやドライバがシェルの初期化状態を問い合わせるためのヘルパ */
int shell_is_initialized(void)
{
	return shell_state.initialized;
}
