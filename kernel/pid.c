#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/sched.h>
#include <kfs/slab.h>

/* PID管理用の定数 */
#define PID_MAX_DEFAULT 32768 /* デフォルト最大PID（Linux 6.18互換） */

/* PID_MAX_DEFAULT個のPID管理に必要なunsigned long配列の要素数を計算するもの */
#define PIDMAP_ENTRIES ((PID_MAX_DEFAULT + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long)))

/* PIDビットマップ */
static struct
{
	unsigned long page[PIDMAP_ENTRIES]; /* ビットマップページ（各ビットが1つのPIDを表す） */
	int nr_free;						/* 空きPID数 */
} pidmap = {
	.page =
		{
			1,
		}, /* PID 0は予約済み（init_task用） */
	.nr_free = PID_MAX_DEFAULT - 1,
};

/* PID割り当ての開始位置（0は予約済み） */
static int last_pid = 0;

/** 新しいPIDを割り当てる
 * @brief 空きPIDを検索して割り当てる
 * @return 割り当てられたPID構造体，失敗時NULL
 */
struct pid *alloc_pid(void)
{
	struct pid *pid_struct;
	int pid_nr;
	int offset, bit, i;

	/* 空きPIDがない */
	if (pidmap.nr_free == 0)
	{
		return NULL;
	}

	/* 次の空きPIDを検索（ラウンドロビン） */
	pid_nr = -1;
	for (i = 0; i < PID_MAX_DEFAULT; i++)
	{
		int test_pid = (last_pid + i + 1) % PID_MAX_DEFAULT;
		offset = test_pid / (8 * sizeof(long)); // pidmap.page配列のインデックス
		bit = test_pid % (8 * sizeof(long));	// pidmap.page配列内のビット位置

		/* このPIDが空いているか確認 */
		if (!(pidmap.page[offset] & (1UL << bit)))
		{
			pid_nr = test_pid;
			break;
		}
	}

	if (pid_nr < 0)
	{
		return NULL; /* 空きが見つからない */
	}

	/* PIDビットを立てる */
	offset = pid_nr / (8 * sizeof(long));
	bit = pid_nr % (8 * sizeof(long));
	pidmap.page[offset] |= (1UL << bit);
	pidmap.nr_free--;
	last_pid = pid_nr;

	/* pid構造体を割り当て */
	pid_struct = kmalloc(sizeof(struct pid));
	if (!pid_struct)
	{
		/* メモリ不足：PIDビットを戻す */
		pidmap.page[offset] &= ~(1UL << bit);
		pidmap.nr_free++;
		return NULL;
	}

	/* pid構造体を初期化 */
	pid_struct->count = 1;	 /* 参照カウント1で開始 */
	pid_struct->level = 0;	 /* 単一namespace（レベル0） */
	pid_struct->inum = 1;	 /* ルートnamespace ID（固定） */
	pid_struct->nr = pid_nr; /* PID番号を保存 */

	/* tasks配列を初期化（Phase 14でスレッドグループ管理に使用） */
	for (i = 0; i < PIDTYPE_MAX; i++)
	{
		pid_struct->tasks[i].first = NULL;
	}

	return pid_struct;
}

/** PIDを解放する
 * @brief 参照カウントをデクリメントし，0になったら解放する
 * @param pid_struct 解放するPID構造体
 */
void put_pid(struct pid *pid_struct)
{
	int pid_nr;
	int offset, bit;

	if (!pid_struct)
	{
		return;
	}

	/* 参照カウントをデクリメント */
	pid_struct->count--;
	if (pid_struct->count > 0)
	{
		return; /* まだ参照されている */
	}

	/* PID番号を取得 */
	pid_nr = pid_struct->nr;

	/* PIDビットをクリア */
	offset = pid_nr / (8 * sizeof(long));
	bit = pid_nr % (8 * sizeof(long));
	pidmap.page[offset] &= ~(1UL << bit);
	pidmap.nr_free++;

	/* pid構造体を解放 */
	kfree(pid_struct);
}

/* PIDハッシュテーブル（簡易版、Phase 2で使用） */
#define PID_HASH_SIZE 256
static struct hlist_head pid_hash[PID_HASH_SIZE] __attribute__((unused));

/** PID用ハッシュ関数
 * @param pid プロセスID
 * @return ハッシュテーブルのインデックス
 */
static inline int pid_hashfn(pid_t pid)
{
	return pid % PID_HASH_SIZE;
}

/** プロセスをPIDハッシュテーブルに追加する
 * @param task 追加するプロセス
 *
 * PIDからの高速検索のため
 * Phase 2で完全実装
 */
void hash_pid(struct task_struct *task)
{
	(void)task; /* Phase 2で使用 */
				/* Phase 2で完全実装 */
}

/** PIDからプロセスを検索する
 * @brief PIDハッシュテーブルから検索する
 * @param pid 検索するプロセスID
 * @return 見つかったtask_struct（プロセス）、見つからない場合NULL
 * @note Phase 1では簡易実装、Phase 2で完全実装予定
 * @note Phase 14以降、スレッドも検索対象になる
 */
struct task_struct *find_task_by_pid(pid_t pid)
{
	(void)pid; /* Phase 2で使用 */
	/* 簡易実装：グローバルタスクリストから検索 */
	/* Phase 2でハッシュテーブル検索に更新 */
	return NULL; /* Phase 2で実装 */
}
