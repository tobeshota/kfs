#ifndef _KFS_SCHED_H
#define _KFS_SCHED_H

#include <kfs/list.h>
#include <kfs/mm_types.h>
#include <kfs/rbtree.h>
#include <kfs/stdint.h>

/* プロセスID型 */
typedef int pid_t;

/* ユーザーID型 */
typedef unsigned int uid_t;

/** カーネル内部UID型
 * @note 将来的なuser namespaceの対応を見越したラッパー型
 */
typedef struct
{
	uid_t val; /* ユーザーID値 */
} kuid_t;

/** 参照カウンタ型
 * @note 単一CPUを前提とする場合，通常のintと同じ
 */
typedef int refcount_t;

/* POSIX Capability型 */
typedef struct
{
	uint32_t cap[2]; /* Capabilityビットマスク（64ビット = u32 x 2） */
} kernel_cap_t;

/* POSIX Capability */
#define CAP_CHOWN 0		   /* ファイル所有者変更 */
#define CAP_DAC_OVERRIDE 1 /* DAC（任意アクセス制御）を無視 */
#define CAP_KILL 5		   /* 任意プロセスへシグナル送信 */
#define CAP_SETUID 7	   /* UID設定 */
#define CAP_SYS_ADMIN 21   /* システム管理操作 */

/* 全Capability有効 */
#define CAP_FULL_SET ((kernel_cap_t){{0xffffffff, 0xffffffff}})
/* 全Capability無効 */
#define CAP_EMPTY_SET ((kernel_cap_t){{0, 0}})

/** シグナル共有情報
 * Linux 6.18ではスレッドグループで共有されるシグナル情報
 * Phase 4で詳細実装予定、今は最小限
 */
struct signal_struct
{
	atomic_t sigcnt; /* 参照カウント */
};

/** 保留中シグナル
 * Phase 4で詳細実装予定、今は最小限
 */
struct sigpending
{
	struct list_head list; /* シグナルキュー */
	uint64_t signal;	   /* 保留中シグナルビットマスク */
};

/** CFS用スケジューリングエンティティ
 * Phase 7で実装予定、今は構造のみ定義
 */
struct sched_entity
{
	unsigned long load;		 /* エンティティの負荷重み */
	struct rb_node run_node; /* CFSのrb-treeノード（vruntimeでソート） */
	unsigned int on_rq;		 /* ランキューに登録されているか */
	uint64_t vruntime;		 /* 仮想実行時間（ナノ秒単位） */
};

/** プロセスの状態
 * @brief task_struct->__stateの値
 * @see Linux 6.18 include/linux/sched.h
 */
#define TASK_RUNNING 0x0000			/* 実行中/実行可能 */
#define TASK_INTERRUPTIBLE 0x0001	/* 割り込み可能スリープ */
#define TASK_UNINTERRUPTIBLE 0x0002 /* 割り込み不可スリープ */
#define __TASK_STOPPED 0x0004		/* 停止（SIGSTOP等） */
#define __TASK_TRACED 0x0008		/* トレース中（ptrace） */
#define TASK_PARKED 0x0040			/* パーク状態 */
#define TASK_DEAD 0x0080			/* 終了状態 */
#define TASK_WAKEKILL 0x0100		/* SIGKILLで起床可能 */
#define TASK_WAKING 0x0200			/* 起床処理中 */

/* タスクフラグ(task_struct->flagsの値) */
#define PF_EXITING 0x00000004 /* 終了中 */
#define PF_KTHREAD 0x00200000 /* カーネルスレッド */

/* プロセス名の最大長（Linux 6.18互換） */
#define TASK_COMM_LEN 16

/** プロセス/スレッド記述子
 * @brief プロセス/スレッドの全情報を保持する中核構造体
 */
struct task_struct
{
	/* 状態管理 */
	volatile unsigned int __state; /* プロセス状態（TASK_RUNNING等） */
	void *stack;				   /* カーネルスタックへのポインタ */
	unsigned int flags;			   /* プロセスフラグ（PF_*） */

	/* メモリ管理 */
	struct mm_struct *mm; /* メモリ記述子 */

	/* プロセスID */
	pid_t pid; /* プロセスID */

	/* プロセス階層 */
	struct task_struct *parent; /* 親プロセス */
	struct list_head children;	/* 子プロセスリスト */
	struct list_head sibling;	/* 兄弟プロセスリンク */
	struct list_head tasks;		/* グローバルタスクリストリンク */

	/* 所有者・権限 */
	kuid_t uid;					/* 実ユーザーID */
	kuid_t euid;				/* 実効ユーザーID（権限チェック用） */
	kernel_cap_t cap_effective; /* 有効なCapability */

	/* シグナル */
	struct signal_struct *signal; /* シグナル共有情報 */
	struct sigpending pending;	  /* 保留シグナル */

	/* スケジューリング（CFS用） */
	struct sched_entity se; /* スケジューリングエンティティ（se.run_node, se.vruntimeを使用） */

	/* プロセス名 */
	char comm[TASK_COMM_LEN]; /* プロセス名（最大16バイト） */
};

#endif /* _KFS_SCHED_H */
