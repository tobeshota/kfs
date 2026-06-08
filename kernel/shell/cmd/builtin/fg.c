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

/** fg/bg 共通形式のジョブID（例: %1）を整数に変換する
 * @param args コマンド引数文字列
 * @param job_id 変換結果の格納先
 * @return 0: 成功, -1: 解析失敗
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

/** ジョブをフォアグラウンドへ移し、終了または停止まで待機する
 * @param args ジョブ指定引数（例: "%1"）
 */
void cmd_fg(void *args)
{
	int job_id;

	if (parse_job_id((const char *)args, &job_id) < 0)
	{
		printf("Usage: fg %%<jobid>\n");
		return;
	}
	if (shell_jobs_fg(job_id) < 0)
	{
		printf("fg: no such job\n");
	}
}
