#include <kfs/errno.h>
#include <kfs/printk.h>
#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

struct signal_name
{
	int signo;
	const char *name;
};

static const struct signal_name signal_names[] = {
	{SIGHUP, "SIGHUP"},	  {SIGINT, "SIGINT"},	{SIGQUIT, "SIGQUIT"}, {SIGILL, "SIGILL"},	{SIGTRAP, "SIGTRAP"},
	{SIGABRT, "SIGABRT"}, {SIGBUS, "SIGBUS"},	{SIGFPE, "SIGFPE"},	  {SIGKILL, "SIGKILL"}, {SIGUSR1, "SIGUSR1"},
	{SIGSEGV, "SIGSEGV"}, {SIGUSR2, "SIGUSR2"}, {SIGPIPE, "SIGPIPE"}, {SIGALRM, "SIGALRM"}, {SIGTERM, "SIGTERM"},
};

static const int signal_name_count = sizeof(signal_names) / sizeof(signal_names[0]);

/** 文字列の先頭トークンを整数として読み取る
 * @param cursor 入力位置（呼び出し後はトークン末尾を指す）
 * @param out    変換結果
 * @return 成功時は0、失敗時は-1
 * @example
 * const char *input = "  123 456";
 * int value;
 * if (parse_int_token(&input, &value) == 0) {
 *    // value == 123, input は " 456" を指す
 * }
 */
static int parse_int_token(const char **cursor, int *out)
{
	const char *current = *cursor;
	const char *end;
	char token[32];

	/* 先頭の空白をスキップ */
	while (*current == ' ')
	{
		current++;
	}

	if (*current == '\0')
	{
		return -1;
	}

	end = strchr(current, ' ');
	size_t len = end ? (size_t)(end - current) : strlen(current);
	if (len == 0 || len >= sizeof(token))
	{
		return -1;
	}
	if (!(current[0] >= '0' && current[0] <= '9') && current[0] != '+' && current[0] != '-')
	{
		return -1;
	}
	for (size_t i = 1; i < len; i++)
	{
		if (current[i] < '0' || current[i] > '9')
		{
			return -1;
		}
	}

	memcpy(token, current, len);
	token[len] = '\0';

	*out = atoi(token);
	*cursor = end ? end : current + len;
	return 0;
}

static const char *skip_spaces(const char *cursor)
{
	while (*cursor == ' ')
	{
		cursor++;
	}
	return cursor;
}

static int parse_pid_token(const char **cursor, int *out)
{
	*cursor = skip_spaces(*cursor);
	return parse_int_token(cursor, out);
}

static int parse_signal_option(const char **cursor, int *sig)
{
	const char *current = skip_spaces(*cursor);
	char token[32];
	const char *end;
	size_t len;

	if (*current != '-')
	{
		return 0;
	}
	current++;
	if (*current == '\0')
	{
		return -1;
	}
	if (*current == '-')
	{
		end = strchr(current, ' ');
		len = end ? (size_t)(end - current) : strlen(current);
		if (len == 6 && strncmp(current, "--help", 6) == 0)
		{
			*cursor = end ? end : current + len;
			return 1;
		}
		return -1;
	}

	end = strchr(current, ' ');
	len = end ? (size_t)(end - current) : strlen(current);
	if (len == 0 || len >= sizeof(token))
	{
		return -1;
	}
	for (size_t i = 0; i < len; i++)
	{
		if (current[i] < '0' || current[i] > '9')
		{
			return -1;
		}
	}

	memcpy(token, current, len);
	token[len] = '\0';
	*sig = atoi(token);
	*cursor = end ? end : current + len;
	return 1;
}

static int is_token_end(const char *cursor)
{
	cursor = skip_spaces(cursor);
	return *cursor == '\0';
}

static void print_kill_help(void)
{
	printf("Usage: kill [options] <pid>\n");
	printf("Options:\n");
	printf("  -l            list supported signals\n");
	printf("  -<number>     send the specified signal number\n");
	printf("  --help        show this help\n");
	printf("Default signal: SIGTERM (15)\n");
	printf("Examples:\n");
	printf("  kill 1234\n");
	printf("  kill -9 1234\n");
}

static void print_signal_list(void)
{
	/* Print in columns: " N) SIGNAME" with 5 entries per line for readability */
	const int per_line = 5;
	for (int i = 0; i < signal_name_count; i++)
	{
		printf("%2d) %10s%s", signal_names[i].signo, signal_names[i].name,
			   ((i % per_line) == (per_line - 1) || i == signal_name_count - 1) ? "\n" : " ");
	}
}

static int send_kill_signal(int pid, int sig)
{
	int ret = kill((pid_t)pid, sig);

	if (ret == 0)
	{
		printf("kill: sent signal %d to pid %d\n", sig, pid);
		return 0;
	}
	if (ret == -ESRCH)
	{
		printf("kill: no such process: %d\n", pid);
		return -1;
	}
	if (ret == -EINVAL)
	{
		printf("kill: invalid signal: %d\n", sig);
		return -1;
	}
	printf("kill: failed with error %d\n", ret);
	return -1;
}

/** kill コマンド
 * @brief `kill [options] <pid>` でプロセスにシグナルを送る
 */
void cmd_kill(void *arg)
{
	const char *cursor = arg;
	int sig = SIGTERM;
	int pid;
	int signal_token;

	cursor = skip_spaces(cursor);
	if (*cursor == '\0')
	{
		print_kill_help();
		return;
	}
	if (strncmp(cursor, "--help", 6) == 0 && is_token_end(cursor + 6))
	{
		print_kill_help();
		return;
	}
	if (strncmp(cursor, "-l", 2) == 0 && is_token_end(cursor + 2))
	{
		print_signal_list();
		return;
	}

	if (*cursor == '-' && cursor[1] != '\0' && cursor[1] != '-')
	{
		if (parse_signal_option(&cursor, &sig) <= 0)
		{
			print_kill_help();
			return;
		}
		if (parse_pid_token(&cursor, &pid) != 0 || !is_token_end(cursor))
		{
			print_kill_help();
			return;
		}
		send_kill_signal(pid, sig);
		return;
	}

	if (parse_pid_token(&cursor, &pid) != 0)
	{
		print_kill_help();
		return;
	}
	cursor = skip_spaces(cursor);
	if (*cursor != '\0')
	{
		if (parse_int_token(&cursor, &signal_token) != 0 || !is_token_end(cursor))
		{
			print_kill_help();
			return;
		}
		sig = signal_token;
	}
	send_kill_signal(pid, sig);
}
