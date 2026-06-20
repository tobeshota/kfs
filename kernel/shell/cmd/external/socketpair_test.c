#include <kfs/shell.h>
#include <kfs/socket.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

/** 空白だけで構成される引数か確認する
 * @param args コマンド引数
 * @return 1=空または空白のみ，0=余分な引数あり
 */
static int socketpair_test_args_empty(const char *args)
{
	while (*args == ' ')
	{
		args++;
	}
	return *args == '\0';
}

/** 親子process間のsocketpair通信を確認する
 * @param arg コマンド引数。引数なしのみ受け付ける
 * @details 以下の出力を確認する
 * ```
 * kfs $ socketpair_test
 * Hello, hello!
 * ```
 * これは，親プロセスが子プロセスから送信された "hello" を受信し，
 * Hello, hello! と出力することを意味する．
 */
void cmd_socketpair_test(void *arg)
{
	const char *args = (const char *)arg;
	/* 引数が空でない場合は使用方法を表示する */
	if (!socketpair_test_args_empty(args))
	{
		printf("Usage: socketpair_test\n");
		return;
	}

	int sv[2]; /* ソケットペアのファイルディスクリプタ */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
	{
		printf("socketpair_test: socketpair failed\n");
		return;
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		printf("socketpair_test: fork failed\n");
		return;
	}
	if (pid == 0)
	{
		/* 子プロセス */
		exit(write(sv[1], "hello", 5) == 5 ? 0 : 1);
	}
	else
	{
		/* 親プロセス */
		char buf[6] = {0};
		int status;
		int received = read(sv[0], buf, 5);
		if (waitpid(pid, &status, 0) < 0 || received != 5 || memcmp(buf, "hello", 5) != 0)
		{
			printf("socketpair_test: communication failed\n");
			return;
		}
		printf("Hello, %s!\n", buf); /* Hello, hello! */
	}
}
