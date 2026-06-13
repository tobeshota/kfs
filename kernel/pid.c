#include <kfs/errno.h>
#include <kfs/pid.h>
#include <kfs/printk.h>
#include <kfs/sched.h>
#include <kfs/slab.h>

/* PID管理用の定数 */
#define PID_MAX_DEFAULT 32768 /* デフォルト最大PID（Linux 6.18互換） */

/* PID_MAX_DEFAULT個のPID管理に必要なunsigned long配列の要素数を計算するもの */
#define PIDMAP_ENTRIES ((PID_MAX_DEFAULT + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long)))

/** PIDビットマップ
 * @brief 各PID番号の使用状況(使用済か未使用か)を表すビットマップ
 */
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
	int offset, bit, i, test_pid;

	/* 空きPIDがない */
	if (pidmap.nr_free == 0)
	{
		return NULL;
	}

	/* 最小の空きPIDを検索（常に小さい番号を優先して再利用する） */
	pid_nr = -1;
	for (test_pid = 1; test_pid < PID_MAX_DEFAULT; test_pid++)
	{
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

	/* tasks配列を初期化（スレッドグループ管理で使用） */
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

/* PIDハッシュテーブル（現在は簡易実装） */
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
 * PID ハッシュテーブルへの登録は未実装
 */
void hash_pid(struct task_struct *task)
{
	(void)task; /* PID ハッシュテーブル登録を実装するまで未使用 */
}

/** PID管理の初期化
 * @note 現在は静的初期化で十分なので何もしない
 */
void pid_init(void)
{
	/* 動的な PID 管理が必要になったら初期化処理を追加する */
}
