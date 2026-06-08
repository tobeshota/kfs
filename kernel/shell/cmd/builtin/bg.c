#include <kfs/shell.h>
#include <kfs/stdio.h>

/** 先頭の空白文字を読み飛ばして次のトークン先頭を返す
 * @param s 入力文字列
 * @return 先頭空白を除いた位置のポインタ
 */
static const char *skip_spaces(const char *s)
{
	while (*s == ' ')
	{
		s++;
	}
	return s;
}

/** ジョブIDを解析する
 * @param args コマンド引数文字列（例: "%1"）
 * @param job_id 解析したジョブIDを格納するポインタ
 * @return 0: 成功, -1: 解析エラー
 */
static int parse_job_id(const char *args, int *job_id)
{
	const char *p = skip_spaces(args);
	int n = 0;

	if (*p == '%')
	{
		p++;
	}
	if (*p < '0' || *p > '9')
	{
		return -1;
	}

	while (*p >= '0' && *p <= '9')
	{
		n = n * 10 + (*p - '0');
		p++;
	}
	*job_id = n;
	return 0;
}

/** 停止中ジョブをバックグラウンドで再開する
 * @param args ジョブ指定引数（例: "%1"）
 */
void cmd_bg(void *args)
{
	int job_id;

	if (parse_job_id((const char *)args, &job_id) < 0)
	{
		printf("Usage: bg %%<jobid>\n");
		return;
	}

	if (shell_jobs_bg(job_id) < 0)
	{
		printf("bg: no such job\n");
		return;
	}
	printf("[%d] Continued\n", job_id);
}
