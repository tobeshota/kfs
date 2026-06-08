#include <kfs/console.h>
#include <kfs/errno.h>
#include <kfs/sched.h>
#include <kfs/serial.h>
#include <kfs/signal.h>
#include <kfs/string.h>
#include <kfs/tty.h>

/* 1行入力バッファの最大長（終端NULを含む） */
#define TTY_LINE_MAX 256

/* TTYの行状態を保持する構造体 */
struct tty_line_state
{
	char input_line[TTY_LINE_MAX];	 /* 入力中の1行分のバッファ */
	unsigned int input_line_len;	 /* 入力中の行の長さ */
	unsigned int input_cursor;		 /* 入力中の行のカーソル位置 */
	char ready_line[TTY_LINE_MAX];	 /* 読み取り可能な行 */
	int line_ready;					 /* 行が準備できているかどうか */
	struct task_struct *line_waiter; /* 行を待っているタスク */
	int echo_enabled;				 /* エコーが有効かどうか */
};

/* 仮想コンソールごとのTTY行状態 */
static struct tty_line_state tty_line_states[KFS_VIRTUAL_CONSOLE_COUNT];

/** コンソール番号に対応するTTY行状態を取得する
 * @param console_index 仮想コンソール番号
 * @return 対応するTTY行状態へのポインタ
 * @note 範囲外の番号はコンソール0へフォールバックする
 */
static struct tty_line_state *tty_state_for_console(size_t console_index)
{
	if (console_index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return &tty_line_states[0];
	}
	return &tty_line_states[console_index];
}

/** 指定コンソールが現在アクティブ表示中かを返す
 * @param console_index 仮想コンソール番号
 * @return アクティブなら1、それ以外は0
 */
static int tty_console_is_active(size_t console_index)
{
	return console_index == kfs_terminal_active_console();
}

/** 入力中の行を確定して読み取り可能状態にする
 * @param state 対象TTY行状態
 * @details input_line を ready_line にコピーし、必要なら待機タスクを起床する
 */
static void tty_publish_line(struct tty_line_state *state)
{
	unsigned int i;

	for (i = 0; i < state->input_line_len && i + 1 < TTY_LINE_MAX; i++)
	{
		state->ready_line[i] = state->input_line[i];
	}
	state->ready_line[i] = '\0';
	state->line_ready = 1;
	state->input_line_len = 0;
	state->input_cursor = 0;
	state->input_line[0] = '\0';

	/* 行を待っているタスクがあれば起床させる */
	if (state->line_waiter)
	{
		wake_up_process(state->line_waiter);
		state->line_waiter = NULL;
	}
}

/** カーソル位置に1文字挿入する
 * @param state 対象TTY行状態
 * @param ch 挿入する文字
 */
static void tty_insert_char_at_cursor(struct tty_line_state *state, char ch)
{
	if (state->input_line_len + 1 >= TTY_LINE_MAX)
	{
		return;
	}

	if (state->input_cursor < state->input_line_len)
	{
		/* カーソルが行の途中にある場合、既存の文字を右にシフトして新しい文字を挿入する */
		memmove(&state->input_line[state->input_cursor + 1], &state->input_line[state->input_cursor],
				state->input_line_len - state->input_cursor + 1);
		state->input_line[state->input_cursor] = ch;
	}
	else
	{
		/* カーソルが行の末尾にある場合、文字を追加する */
		state->input_line[state->input_cursor] = ch;
		state->input_line[state->input_cursor + 1] = '\0';
	}

	state->input_line_len++;
	state->input_cursor++;
}

/* 全仮想コンソールのTTY状態を初期化する */
void tty_reset(void)
{
	for (size_t i = 0; i < KFS_VIRTUAL_CONSOLE_COUNT; ++i)
	{
		tty_line_states[i].input_line_len = 0;
		tty_line_states[i].input_cursor = 0;
		tty_line_states[i].input_line[0] = '\0';
		tty_line_states[i].ready_line[0] = '\0';
		tty_line_states[i].line_ready = 0;
		tty_line_states[i].line_waiter = NULL;
		tty_line_states[i].echo_enabled = 1;
	}
}

/** 指定コンソールの1行入力を読み取る
 * @param console_index 仮想コンソール番号
 * @param buf 読み取り先バッファ
 * @param size 読み取り先バッファサイズ
 * @return 読み取った文字数
 * @note 行が確定していない場合は待機する
 */
long tty_read_line_for_console(size_t console_index, char *buf, unsigned int size)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	if (size == 0)
	{
		return 0;
	}

	while (1)
	{
		/** バックグラウンドプロセスが TTY から読み取ろうとした場合は SIGTTIN を送信する
		 * @brief 呼び出し元プロセスのプロセスグループが
		 *        フォアグラウンドプロセスグループでない場合，
		 *        呼び出し元プロセスに対してSIGTTINを送信する
		 */
		pid_t fg = kfs_terminal_get_foreground_pgrp_for_console(console_index);
		if (fg != 0 && current->pgrp != fg)
		{
			send_signal(SIGTTIN, current);
			return -EINTR;
		}

		__asm__ volatile("cli");

		if (state->line_ready)
		{
			unsigned int copy_len = 0;

			while (copy_len + 1 < size && state->ready_line[copy_len] != '\0')
			{
				buf[copy_len] = state->ready_line[copy_len];
				copy_len++;
			}
			buf[copy_len] = '\0';
			state->line_ready = 0;
			__asm__ volatile("sti");
			return (long)copy_len;
		}

		state->line_waiter = current;
		current->__state = TASK_INTERRUPTIBLE;
		__asm__ volatile("sti");
		schedule();
	}
}

/** 指定コンソールの入力バッファに1文字追加する
 * @param console_index 仮想コンソール番号
 * @param ch 追加する文字
 */
void tty_input_char_for_console(size_t console_index, char ch)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	if (state->input_line_len + 1 >= TTY_LINE_MAX)
	{
		return;
	}

	tty_insert_char_at_cursor(state, ch);
	if (state->echo_enabled && tty_console_is_active(console_index))
	{
		terminal_write_console(console_index, &ch, 1);
		serial_write(&ch, 1);
	}
}

/** 指定コンソールの入力に対してバックスペースを適用する
 * @param console_index 仮想コンソール番号
 */
void tty_handle_backspace_for_console(size_t console_index)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	if (state->input_cursor == 0 || state->input_line_len == 0)
	{
		return;
	}

	memmove(&state->input_line[state->input_cursor - 1], &state->input_line[state->input_cursor],
			state->input_line_len - state->input_cursor + 1);
	state->input_cursor--;
	state->input_line_len--;

	if (state->echo_enabled && tty_console_is_active(console_index))
	{
		kfs_terminal_cursor_left();
		terminal_delete_char();
		serial_write("\b \b", 3);
	}
}

/** 指定コンソールで改行入力を処理し、現在行を確定する
 * @param console_index 仮想コンソール番号
 */
void tty_handle_enter_for_console(size_t console_index)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	if (state->echo_enabled && tty_console_is_active(console_index))
	{
		terminal_write_console(console_index, "\n", 1);
		serial_write("\n", 1);
	}
	tty_publish_line(state);
}

/** 指定コンソールの入力カーソルを左へ移動する
 * @param console_index 仮想コンソール番号
 */
void tty_handle_cursor_left_for_console(size_t console_index)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	if (state->input_cursor == 0)
	{
		return;
	}

	state->input_cursor--;
	if (state->echo_enabled && tty_console_is_active(console_index))
	{
		kfs_terminal_cursor_left();
	}
}

/** 指定コンソールの入力カーソルを右へ移動する
 * @param console_index 仮想コンソール番号
 */
void tty_handle_cursor_right_for_console(size_t console_index)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	if (state->input_cursor >= state->input_line_len)
	{
		return;
	}

	state->input_cursor++;
	if (state->echo_enabled && tty_console_is_active(console_index))
	{
		kfs_terminal_cursor_right();
	}
}

/** 指定コンソールの入力中行を破棄する
 * @param console_index 仮想コンソール番号
 * @param publish_empty 1なら空行として確定し待機読取を解除する
 */
void tty_discard_input_for_console(size_t console_index, int publish_empty)
{
	struct tty_line_state *state = tty_state_for_console(console_index);

	state->input_line_len = 0;
	state->input_cursor = 0;
	state->input_line[0] = '\0';

	if (publish_empty)
	{
		tty_publish_line(state);
	}
}

/** 指定コンソールのエコー有効/無効を設定する
 * @brief エコーが有効な場合，入力された文字が端末に表示される．
 *        エコーが無効な場合，入力バッファには入るが，端末には表示されない（パスワード入力などに利用）．
 * @param console_index 仮想コンソール番号
 * @param enabled 0以外で有効，0で無効
 * @return 成功時0，範囲外コンソール番号で-EINVAL
 */
int tty_set_echo_for_console(size_t console_index, int enabled)
{
	if (console_index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return -EINVAL;
	}
	tty_line_states[console_index].echo_enabled = enabled ? 1 : 0;
	return 0;
}

/** 指定コンソールのエコー設定を取得する
 * @param console_index 仮想コンソール番号
 * @return 0または1、範囲外コンソール番号で-EINVAL
 */
int tty_get_echo_for_console(size_t console_index)
{
	if (console_index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return -EINVAL;
	}
	return tty_line_states[console_index].echo_enabled;
}
