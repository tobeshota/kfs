#include <kfs/exec.h>
#include <kfs/prctl.h>
#include <kfs/shell.h>
#include <kfs/signal.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

#define SHELL_PROMPT "kfs $ " /* シェルプロンプト文字列 */
#define CMD_BUFFER_SIZE 256	  /* コマンドバッファのサイズ */
#define PS2_STATUS_PORT 0x64 /* PS/2 コントローラのペリフェラルから受け取るステータスレジスタのポート番号 */
#define PS2_RESET_COMMAND 0xFE /* PS/2 コントローラのリセットコマンド */

/* シェルの状態を保持する構造体 */
static struct
{
	char cmd_buffer[CMD_BUFFER_SIZE]; /* 入力されたコマンド文字列を格納 */
	size_t cmd_len;					  /* 現在のコマンド長 */
	int initialized;				  /* 初期化済みフラグ */
} shell_state;

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

static void execute_external_command(const char *cmd, shell_cmd_fn fn, const char *args, int foreground)
{
	extern int shell_jobs_add(pid_t pid, pid_t pgrp, const char *cmd, int stopped);
	int status;

	pid_t pid = fork();
	if (pid < 0)
	{
		printf("Failed to fork process\n");
		return;
	}
	else if (pid == 0)
	{
		__exec_fn(cmd, fn, (void *)args);
	}
	else
	{
		/* 親が子をプロセスグループリーダーにする */
		setpgid(pid, pid);

		if (foreground)
		{
			/* 端末のフォアグラウンドプロセスグループを子プロセスグループに移す */
			tcsetpgrp(0, pid);
			status = 0;
			if (waitpid(pid, &status, WUNTRACED) > 0)
			{
				if (WIFSTOPPED(status))
				{
					int job_id = shell_jobs_add(pid, pid, cmd, 1);
					if (job_id > 0)
					{
						printf("[%d] Stopped %s\n", job_id, cmd);
					}
				}
			}

			/* 終了後は shell に foreground を戻す */
			tcsetpgrp(0, getpgrp());
		}
		else
		{
			int job_id = shell_jobs_add(pid, pid, cmd, 0);
			if (job_id > 0)
			{
				printf("[%d] %d\n", job_id, (int)pid);
			}
		}
	}
}

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

	shell_cmd_fn fn;
	const char *args;
	enum shell_cmd_mode mode;
	if (cmd_lookup(cmd, &fn, &args, &mode) == -1)
	{
		printf("Unknown command: %s\n", cmd);
		return;
	}
	else if (mode == SHELL_CMD_BUILTIN)
	{
		fn((void *)args);
	}
	else
	{
		execute_external_command(cmd, fn, args, foreground);
	}
}

/* 非ブロッキングでゾンビ子プロセスを回収する。 */
static void reap_zombie_children(void)
{
	int reap_status;

	while (1)
	{
		pid_t r = waitpid(-1, &reap_status, WNOHANG | WUNTRACED | WCONTINUED);
		if (r <= 0)
		{
			break;
		}

		extern void shell_jobs_on_wait_event(pid_t pid, int wait_status);
		shell_jobs_on_wait_event(r, reap_status);
	}
}

/** 旧 keyboard handler 互換 shim
 * @param c 入力された文字（通常文字、'\n', '\b', 制御文字など）
 * @return 処理した場合は1、処理しなかった場合は0
 * @note 現在の実行経路は shell_run() の read(0, ...) であり、この関数は
 *       主に旧 unit test 互換のために最小限の行バッファ更新だけを行う。
 */
int shell_keyboard_handler(char c)
{
	/* シェルが初期化されていない場合は処理しない（デフォルト動作に任せる） */
	if (!shell_state.initialized)
	{
		return 0;
	}

	/* 旧 line editor の矢印入力は現在 no-op とする */
	if (c == '\x1C' || c == '\x1D')
	{
		return 1;
	}

	/* 互換 shim では Enter で入力行を確定し、バッファだけクリアする。 */
	if (c == '\n' || c == '\r')
	{
		clear_command_buffer();
		return 1;
	}

	/* バックスペースの処理 */
	if (c == '\b' || c == 127) /* 127はDELキー */
	{
		if (shell_state.cmd_len > 0)
		{
			shell_state.cmd_len--;
			shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
		}
		return 1;
	}

	/* バッファオーバーフローを防ぐ */
	if (shell_state.cmd_len >= CMD_BUFFER_SIZE - 1)
	{
		clear_command_buffer();
		return 1;
	}

	/* Ctrl+C は現在の shim では入力行を破棄するだけにする。 */
	if (c == '\x03')
	{
		clear_command_buffer();
		return 1;
	}

	/* '\x03'以外の制御文字は無視（タブなど将来拡張可能） */
	if (c < 32 && c != '\t')
	{
		return 1; /* 処理した（無視） */
	}

	/* 通常文字をバッファに追加する */
	shell_state.cmd_buffer[shell_state.cmd_len++] = c;
	shell_state.cmd_buffer[shell_state.cmd_len] = '\0';
	return 1;
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

	shell_state.initialized = 1;
	cmd_registry_init();
}

/** シェルのメインループ
 * キーボード入力を受け付けてコマンドを処理する
 *
 * この関数はweak symbolとして定義されており、
 * テスト環境では別の実装でオーバーライドできる
 */
__attribute__((weak)) void shell_run(void)
{
	char line[CMD_BUFFER_SIZE];
	int line_len;
	pid_t sid;
	pid_t shell_pgrp;

	shell_init();
	prctl(PR_SET_NAME, (unsigned long)"shell_run", 0, 0, 0);

	/* shell 自身で session/pgrp/foreground を確立する（Phase 5 最小導入） */
	sid = setsid();
	if (sid < 0)
	{
		/* 既にセッションリーダー等で失敗する場合があるため継続する */
	}

	/* shell のプロセスグループを取得してフォアグラウンドに設定する */
	shell_pgrp = getpgrp();
	if (shell_pgrp > 0)
	{
		(void)tcsetpgrp(0, shell_pgrp);
	}

	/* neofetchを出す */
	extern void cmd_neofetch(void *args);
	cmd_neofetch(NULL);

	/* 接続されたTTYを表示する */
	int tty = ttynr();
	if (tty >= 0)
	{
		printf("Connected to tty%u\n", (unsigned int)(tty + 1));
	}

	while (1)
	{
		printf("%s", SHELL_PROMPT);
		line_len = read(0, line, sizeof(line) - 1);
		if (line_len < 0)
		{
			continue;
		}
		line[line_len] = '\0';

		/* 入力待ち中にゾンビ化した子を、次コマンド実行前に先に回収する。 */
		reap_zombie_children();
		execute_command(line);
		reap_zombie_children();
	}
}

/* 単体テストやドライバがシェルの初期化状態を問い合わせるためのヘルパ */
int shell_is_initialized(void)
{
	return shell_state.initialized;
}
