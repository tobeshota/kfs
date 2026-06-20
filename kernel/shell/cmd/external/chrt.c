#include <kfs/errno.h>
#include <kfs/sched.h>
#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

#define CHRT_USAGE                                                                                                     \
	"Usage: chrt -o -p 0 <pid>\n"                                                                                      \
	"       chrt --ext -p 0 <pid>\n"                                                                                   \
	"       chrt -p <pid>\n"

struct chrt_request
{
	int query;	  /* クエリフラグ．スケジューリング情報を取得するかどうかを決める */
	int policy;	  /* スケジューリングポリシー */
	int priority; /* スケジューリング優先度 */
	int pid;	  /* 対象プロセスの PID */
};

/** 空白を読み飛ばす
 * @param s 入力位置
 * @return 空白でない次の位置
 */
static const char *chrt_skip_spaces(const char *s)
{
	while (*s == ' ')
	{
		s++;
	}
	return s;
}

/** 次の空白区切り token を読み取る
 * @param cursor 入力位置。成功時は token 直後へ進む
 * @param out token 出力先
 * @param out_size out のサイズ
 * @return 1=成功, 0=token なしまたは長すぎる
 */
static int chrt_next_token(const char **cursor, char *out, size_t out_size)
{
	const char *start = chrt_skip_spaces(*cursor);
	if (*start == '\0')
	{
		return 0;
	}

	size_t len = 0;
	while (start[len] != '\0' && start[len] != ' ')
	{
		len++;
	}
	if (len == 0 || len >= out_size)
	{
		return 0;
	}
	memcpy(out, start, len);
	out[len] = '\0';
	*cursor = start + len;
	return 1;
}

/** 次の token を整数として読み取る
 * @param cursor 入力位置。成功時は token 直後へ進む
 * @param out 変換結果
 * @return 1=成功, 0=整数 token でない
 */
static int chrt_parse_int_token(const char **cursor, int *out)
{
	char token[32];
	size_t i = 0;

	if (!chrt_next_token(cursor, token, sizeof(token)))
	{
		return 0;
	}
	if (token[0] == '+' || token[0] == '-')
	{
		i = 1;
	}
	if (token[i] == '\0')
	{
		return 0;
	}
	while (token[i] != '\0')
	{
		if (token[i] < '0' || token[i] > '9')
		{
			return 0;
		}
		i++;
	}
	*out = atoi(token);
	return 1;
}

/** policy 名を kfs の SCHED_* に変換する
 * @param token policy token
 * @param policy 変換結果
 * @return 1=成功, 0=不明
 */
static int chrt_parse_policy(const char *token, int *policy)
{
	if (strcmp(token, "-o") == 0 || strcmp(token, "--other") == 0 || strcmp(token, "normal") == 0 ||
		strcmp(token, "other") == 0)
	{
		*policy = SCHED_NORMAL;
		return 1;
	}
	if (strcmp(token, "--ext") == 0 || strcmp(token, "ext") == 0)
	{
		*policy = SCHED_EXT;
		return 1;
	}
	return 0;
}

/** policy値をchrt表示用の名前へ変換する
 * @param policy SCHED_* policy
 * @return policy名
 */
static const char *chrt_policy_name(int policy)
{
	switch (policy)
	{
	case SCHED_NORMAL:
		return "SCHED_OTHER";
	case SCHED_BATCH:
		return "SCHED_BATCH";
	case SCHED_IDLE:
		return "SCHED_IDLE";
	case SCHED_EXT:
		return "SCHED_EXT";
	default:
		return "SCHED_UNKNOWN";
	}
}

/** 対象processのスケジューリング情報を表示する
 * @param pid 対象PID
 */
static void chrt_print_policy(int pid)
{
	int policy = sched_getscheduler((pid_t)pid);

	if (policy == -ESRCH)
	{
		printf("chrt: pid not found: %d\n", pid);
		return;
	}
	if (policy < 0)
	{
		printf("chrt: failed: %d\n", policy);
		return;
	}

	printf("pid %d's current scheduling policy: %s\n", pid, chrt_policy_name(policy));
	printf("pid %d's current scheduling priority: 0\n", pid);
}

/** sched_setscheduler() の戻り値を chrt のエラー表示へ変換する
 * @param ret sched_setscheduler() の戻り値
 * @param pid 対象 PID
 * @return ret が 0 なら 0、それ以外は -1
 */
static int chrt_report_result(int ret, int pid)
{
	char line[64];
	const char *format = "chrt: failed: %d\n";
	int value = ret;

	if (ret == 0)
	{
		format = "chrt: updated pid %d\n";
		value = pid;
	}
	else if (ret == -ESRCH)
	{
		format = "chrt: pid not found: %d\n";
		value = pid;
	}
	else if (ret == -EINVAL)
	{
		format = "chrt: invalid policy or priority\n";
	}
	snprintf(line, sizeof(line), format, value);
	printf("%s", line);
	return ret == 0 ? 0 : -1;
}

/** chrt の引数を解析する
 * @param arg コマンド引数
 * @param request 解析結果
 * @return 1=成功, 0=構文エラー
 */
static int chrt_parse_args(const char *arg, struct chrt_request *request)
{
	const char *cursor = arg;
	char token[32];

	if (!chrt_next_token(&cursor, token, sizeof(token)))
	{
		return 0;
	}

	if (strcmp(token, "-p") == 0)
	{
		request->query = 1;
		if (!chrt_parse_int_token(&cursor, &request->pid))
		{
			return 0;
		}
		return *chrt_skip_spaces(cursor) == 0;
	}

	request->query = 0;
	if (!chrt_parse_policy(token, &request->policy))
	{
		return 0;
	}
	if (!chrt_next_token(&cursor, token, sizeof(token)) || strcmp(token, "-p") != 0)
	{
		return 0;
	}
	if (!chrt_parse_int_token(&cursor, &request->priority) || !chrt_parse_int_token(&cursor, &request->pid))
	{
		return 0;
	}
	return *chrt_skip_spaces(cursor) == 0;
}

/** chrt コマンド
 * @param arg コマンド引数
 * @example
 * `chrt -p 1234`はPID 1234の現在のpolicyとpriorityを表示する．
 * @example
 * `chrt -o -p 0 1234`は
 * PID 1234 のスケジューリングポリシーを SCHED_NORMAL、
 * 優先度を 0 に設定することを意味する．
 * @example
 * `chrt --ext -p 0 5678`は
 * PID 5678 のスケジューリングポリシーを SCHED_EXT、
 * 優先度を 0 に設定することを意味する．
 */
void cmd_chrt(void *arg)
{
	/* argからrequestを解析する */
	struct chrt_request request;
	if (!chrt_parse_args((const char *)arg, &request))
	{
		printf("%s", CHRT_USAGE);
		return;
	}

	if (request.query)
	{
		chrt_print_policy(request.pid);
		return;
	}

	struct sched_param param;
	param.sched_priority = request.priority;

	/* sched_setscheduler() を呼び出して結果を報告する */
	(void)chrt_report_result(sched_setscheduler((pid_t)request.pid, request.policy, &param), request.pid);
}
