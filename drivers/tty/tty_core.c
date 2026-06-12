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
	tcflag_t lflag;					 /* termiosローカルモードフラグ */
	unsigned char cc[NCCS];			 /* termios制御文字 */
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

/** 制御文字配列を初期化する
 * @param cc 初期化する制御文字配列
 */
static void tty_init_default_cc(unsigned char cc[NCCS])
{
	memset(cc, 0, NCCS);
	cc[VINTR] = 0x03;  /* Ctrl-C */
	cc[VQUIT] = 0x1C;  /* Ctrl-\ */
	cc[VERASE] = 0x7F; /* DEL */
	cc[VKILL] = 0x15;  /* Ctrl-U */
	cc[VEOF] = 0x04;   /* Ctrl-D */
	cc[VTIME] = 0;	   /* タイムアウト値 */
	cc[VMIN] = 1;	   /* 最小読み取り文字数 */
	cc[VSTART] = 0x11; /* Ctrl-Q */
	cc[VSTOP] = 0x13;  /* Ctrl-S */
	cc[VSUSP] = 0x1A;  /* Ctrl-Z */
}

/** 入力された制御文字chに対応するシグナル番号を返す
 * @param state 対象TTY行状態
 * @param ch 入力文字
 * @return 対応するシグナル番号。シグナル対象でなければ0
 */
static int tty_signal_for_char(struct tty_line_state *state, char ch)
{
	unsigned char uch = (unsigned char)ch;

	/* シグナル制御文字が有効でない場合はシグナルを送らない */
	if (!(state->lflag & ISIG))
	{
		return 0;
	}

	if (state->cc[VINTR] != KFS_VDISABLE && uch == state->cc[VINTR])
	{
		return SIGINT;
	}
	if (state->cc[VSUSP] != KFS_VDISABLE && uch == state->cc[VSUSP])
	{
		return SIGTSTP;
	}
	return 0;
}

/** 端末制御文字に対応するシグナルをフォアグラウンドプロセスグループへ送る
 * @param console_index 仮想コンソール番号
 * @param sig 送信するシグナル番号
 */
static void tty_send_signal_for_console(size_t console_index, int sig)
{
	pid_t fgprg = kfs_terminal_get_foreground_pgrp_for_console(console_index);

	if (fgprg == 0)
	{
		fgprg = current->pgrp;
	}
	if (fgprg > 0)
	{
		(void)kill_pg(fgprg, sig);
	}
}

/** シグナル制御文字の表示と入力行破棄を行う
 * @param console_index 仮想コンソール番号
 * @param ch 入力された制御文字
 */
static void tty_echo_signal_char(size_t console_index, char ch)
{
	char echo[3];

	if (!tty_console_is_active(console_index))
	{
		return;
	}
	echo[0] = '^';
	echo[1] = (char)(((unsigned char)ch) + '@');
	echo[2] = '\n';
	terminal_write_console(console_index, echo, sizeof(echo));
	serial_write(echo, sizeof(echo));
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

/** 読み取り可能バッファへ1文字追加する
 * @param state 対象TTY行状態
 * @param ch 追加する文字
 * @details non-canonical mode では Enter を待たずに入力文字を ready_line へ積み、
 *          read 待機中のタスクがあれば起床する。
 */
static void tty_append_ready_char(struct tty_line_state *state, char ch)
{
	unsigned int len = 0;

	/* 読み取り可能バッファの末尾を探す */
	while (len + 1 < TTY_LINE_MAX && state->ready_line[len] != '\0')
	{
		len++;
	}

	/* バッファがいっぱいの場合は追加できない */
	if (len + 1 >= TTY_LINE_MAX)
	{
		return;
	}

	/* 読み取り可能バッファに文字を追加する */
	state->ready_line[len] = ch;
	state->ready_line[len + 1] = '\0';
	state->line_ready = 1;

	/* 読み取り待機中のタスクがあれば起床させる */
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
		tty_line_states[i].lflag = ICANON | ECHO | ISIG; /* 行入力モード、エコー有効、シグナル有効 */
		tty_init_default_cc(tty_line_states[i].cc);
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
		/** SIGCHLDを保留中のときreadを中断する
		 * @note 子プロセスの状態回収はシェル側で行われる
		 */
		if (current->pending.signal & (1UL << SIGCHLD))
		{
			return -EINTR;
		}

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
			state->ready_line[0] = '\0';
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
	int sig = tty_signal_for_char(state, ch);

	if (sig)
	{
		tty_send_signal_for_console(console_index, sig);
		if (state->lflag & ECHO)
		{
			tty_echo_signal_char(console_index, ch);
		}
		tty_discard_input_for_console(console_index, 1);
		return;
	}

	if (state->input_line_len + 1 >= TTY_LINE_MAX)
	{
		return;
	}

	if (state->lflag & ICANON)
	{
		/* 行入力モードの場合，カーソル位置に文字を挿入する */
		tty_insert_char_at_cursor(state, ch);
	}
	else
	{
		/* 非行入力モードの場合，読み取り可能バッファへ文字を追加する */
		tty_append_ready_char(state, ch);
	}
	if ((state->lflag & ECHO) && tty_console_is_active(console_index))
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

	/* 非行入力モードの場合，バックスペースを読み取り可能バッファへ追加する */
	if (!(state->lflag & ICANON))
	{
		tty_append_ready_char(state, '\b');
		return;
	}

	if (state->input_cursor == 0 || state->input_line_len == 0)
	{
		return;
	}

	memmove(&state->input_line[state->input_cursor - 1], &state->input_line[state->input_cursor],
			state->input_line_len - state->input_cursor + 1);
	state->input_cursor--;
	state->input_line_len--;

	/* エコーが有効な場合，バックスペースを表示する */
	if ((state->lflag & ECHO) && tty_console_is_active(console_index))
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

	/* エコーが有効な場合，改行を表示する */
	if ((state->lflag & ECHO) && tty_console_is_active(console_index))
	{
		terminal_write_console(console_index, "\n", 1);
		serial_write("\n", 1);
	}

	if (state->lflag & ICANON)
	{
		/* 行入力モードの場合，入力中の行を確定して読み取り可能状態にする */
		tty_publish_line(state);
	}
	else
	{
		/* 非行入力モードの場合，改行文字を読み取り可能バッファへ追加する */
		tty_append_ready_char(state, '\n');
	}
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

	/* エコーが有効な場合，カーソルを左に移動する */
	if ((state->lflag & ECHO) && tty_console_is_active(console_index))
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

	/* エコーが有効な場合，カーソルを右に移動する */
	if ((state->lflag & ECHO) && tty_console_is_active(console_index))
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
	if (enabled)
	{
		/* エコーを有効にする */
		tty_line_states[console_index].lflag |= ECHO;
	}
	else
	{
		/* エコーを無効にする */
		tty_line_states[console_index].lflag &= ~((tcflag_t)ECHO);
	}
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
	return (tty_line_states[console_index].lflag & ECHO) ? 1 : 0;
}

/** 指定コンソールの termios 設定を取得する
 * @param console_index 仮想コンソール番号
 * @param termios 取得した設定の格納先
 * @return 成功時0、範囲外コンソール番号またはNULLポインタで-EINVAL
 */
int tty_get_termios_for_console(size_t console_index, struct termios *termios)
{
	if (console_index >= KFS_VIRTUAL_CONSOLE_COUNT || !termios)
	{
		return -EINVAL;
	}

	memset(termios, 0, sizeof(*termios));
	termios->c_lflag = tty_line_states[console_index].lflag;
	memcpy(termios->c_cc, tty_line_states[console_index].cc, NCCS);

	return 0;
}

/** 指定コンソールの termios 設定を反映する
 * @param console_index 仮想コンソール番号
 * @param termios 反映する設定
 * @return 成功時0、範囲外コンソール番号またはNULLポインタで-EINVAL
 */
int tty_set_termios_for_console(size_t console_index, const struct termios *termios)
{
	if (console_index >= KFS_VIRTUAL_CONSOLE_COUNT || !termios)
	{
		return -EINVAL;
	}

	tty_line_states[console_index].lflag = termios->c_lflag & (ICANON | ECHO | ISIG);
	memcpy(tty_line_states[console_index].cc, termios->c_cc, NCCS);

	return 0;
}

/** 指定コンソールで端末特殊文字のシグナル配送が有効かを返す
 * @param console_index 仮想コンソール番号
 * @return ISIG が有効なら1、それ以外または範囲外コンソール番号なら0
 */
int tty_signal_enabled_for_console(size_t console_index)
{
	if (console_index >= KFS_VIRTUAL_CONSOLE_COUNT)
	{
		return 0;
	}
	return (tty_line_states[console_index].lflag & ISIG) ? 1 : 0;
}
