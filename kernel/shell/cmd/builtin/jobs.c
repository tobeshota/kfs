#include <kfs/shell.h>
#include <kfs/signal.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

#define SHELL_MAX_JOBS 16
#define SHELL_JOB_CMD_LEN 256

/* シェルが管理するジョブエントリ */
struct shell_job
{
	int used;					 /* 使用中フラグ */
	int id;						 /* ジョブID */
	pid_t pid;					 /* プロセスID */
	pid_t pgrp;					 /* プロセスグループID */
	int stopped;				 /* 停止中フラグ */
	char cmd[SHELL_JOB_CMD_LEN]; /* コマンド文字列 */
};

static struct shell_job shell_jobs[SHELL_MAX_JOBS]; /* ジョブテーブル */

/** PID からジョブテーブルのエントリを検索する
 * @param pid 検索対象のプロセスID
 * @return 見つかったジョブ、なければ NULL
 */
static struct shell_job *shell_find_job_by_pid(pid_t pid)
{
	for (int i = 0; i < SHELL_MAX_JOBS; i++)
	{
		if (shell_jobs[i].used && shell_jobs[i].pid == pid)
		{
			return &shell_jobs[i];
		}
	}
	return NULL;
}

/** ジョブID からジョブテーブルのエントリを検索する
 * @param job_id 検索対象のジョブID
 * @return 見つかったジョブ、なければ NULL
 */
static struct shell_job *shell_find_job_by_id(int job_id)
{
	for (int i = 0; i < SHELL_MAX_JOBS; i++)
	{
		if (shell_jobs[i].used && shell_jobs[i].id == job_id)
		{
			return &shell_jobs[i];
		}
	}
	return NULL;
}

/** 未使用の最小ジョブIDを割り当てる
 * @return 未使用の最小ジョブID、空きIDがない場合は -1
 */
static int shell_alloc_job_id(void)
{
	for (int id = 1; id <= SHELL_MAX_JOBS; id++)
	{
		if (!shell_find_job_by_id(id))
		{
			return id;
		}
	}
	return -1;
}

/** ジョブエントリを未使用状態へ戻す
 * @param job 削除対象ジョブ
 */
static void shell_remove_job(struct shell_job *job)
{
	if (!job)
	{
		return;
	}
	job->used = 0;
}

/** 新しいジョブをジョブテーブルへ登録する
 * @param pid プロセスID
 * @param pgrp プロセスグループID
 * @param cmd 表示用コマンド文字列
 * @param stopped 停止状態なら1、実行中なら0
 * @return 登録されたジョブID、満杯時は -1
 */
int shell_jobs_add(pid_t pid, pid_t pgrp, const char *cmd, int stopped)
{
	int job_id = shell_alloc_job_id();
	if (job_id < 0)
	{
		return -1;
	}

	for (int i = 0; i < SHELL_MAX_JOBS; i++)
	{
		if (!shell_jobs[i].used)
		{
			shell_jobs[i].used = 1;
			shell_jobs[i].id = job_id;
			shell_jobs[i].pid = pid;
			shell_jobs[i].pgrp = pgrp;
			shell_jobs[i].stopped = stopped ? 1 : 0;
			strncpy(shell_jobs[i].cmd, cmd, sizeof(shell_jobs[i].cmd) - 1);
			shell_jobs[i].cmd[sizeof(shell_jobs[i].cmd) - 1] = '\0';
			return shell_jobs[i].id;
		}
	}
	return -1;
}

/** waitpid の結果でジョブ状態を更新する
 * @param pid 対象プロセスID
 * @param wait_status waitpid が返したステータス
 */
void shell_jobs_on_wait_event(pid_t pid, int wait_status)
{
	struct shell_job *job = shell_find_job_by_pid(pid);
	if (!job)
	{
		return;
	}

	/* ジョブが停止した場合，停止中フラグを立てる */
	if (WIFSTOPPED(wait_status))
	{
		job->stopped = 1;
		return;
	}

	/* ジョブが再開した場合，停止中フラグを下ろす */
	if (WIFCONTINUED(wait_status))
	{
		job->stopped = 0;
		return;
	}

	/* ジョブが終了した場合，ジョブエントリを未使用状態へ戻す */
	if (WIFEXITED(wait_status) || WIFSIGNALED(wait_status))
	{
		shell_remove_job(job);
	}
}

/** ジョブ一覧を表示する（jobs builtin の実体）
 * @return 常に 0
 */
int shell_jobs_list(void)
{
	for (int i = 0; i < SHELL_MAX_JOBS; i++)
	{
		if (!shell_jobs[i].used)
		{
			continue;
		}
		printf("[%d] %s %s\n", shell_jobs[i].id, shell_jobs[i].stopped ? "Stopped" : "Running", shell_jobs[i].cmd);
	}
	return 0;
}

/** 指定ジョブをフォアグラウンドへ移し、状態変化まで待つ
 * @param job_id 対象ジョブID
 * @return 0: 成功, -1: ジョブ未存在
 */
int shell_jobs_fg(int job_id)
{
	struct shell_job *job = shell_find_job_by_id(job_id);
	if (!job)
	{
		return -1;
	}

	/* ジョブが停止中の場合，再開する */
	if (job->stopped)
	{
		(void)kill(-job->pgrp, SIGCONT);
		job->stopped = 0;
	}

	/* 指定ジョブのプロセスグループjob->pgrpを
	 * 現在の端末のフォアグラウンドプロセスグループに設定する */
	(void)tcsetpgrp(0, job->pgrp);

	/** job_id指定プロセスの終了または停止を待つ．
	 * @note バックグラウンド化shell_jobs_bg(); では，
	 *       job_id指定プロセスの状態変化を待たない
	 */
	int status = 0;
	if (waitpid(job->pid, &status, WUNTRACED) > 0)
	{
		/* waitpid がジョブの状態変化を検知した場合，
		 * ジョブの状態を更新する */
		shell_jobs_on_wait_event(job->pid, status);
	}

	/* 現在の端末の制御を呼び出し元のプロセスグループに戻す */
	(void)tcsetpgrp(0, getpgrp());

	return 0;
}

/** 指定ジョブをバックグラウンドで再開する
 * @param job_id 対象ジョブID
 * @return 0: 成功, -1: ジョブ未存在
 */
int shell_jobs_bg(int job_id)
{
	struct shell_job *job = shell_find_job_by_id(job_id);
	if (!job)
	{
		return -1;
	}

	/* ジョブが停止中の場合，再開する */
	if (job->stopped)
	{
		(void)kill(-job->pgrp, SIGCONT);
		job->stopped = 0;
	}

	/* フォアグラウンド化shell_jobs_fg();と異なり，
	 * バックグラウンド化では，job_id指定プロセスの状態変化をwaitpid)();等で待たない */

	return 0;
}

/* ジョブテーブルを初期化する */
void shell_jobs_reset(void)
{
	memset(shell_jobs, 0, sizeof(shell_jobs));
}

/** jobs コマンドエントリ
 * @param args 未使用
 */
void cmd_jobs(void *args)
{
	(void)args;
	(void)shell_jobs_list();
}
