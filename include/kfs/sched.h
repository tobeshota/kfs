#ifndef _KFS_SCHED_H
#define _KFS_SCHED_H

#include <asm-i386/page.h>
#include <asm-i386/ptrace.h>
#include <kfs/capability.h>
#include <kfs/list.h>
#include <kfs/mm_types.h>
#include <kfs/pid.h>
#include <kfs/rbtree.h>
#include <kfs/signal.h>
#include <kfs/stdint.h>

/** 現在のプロセスが指定 Capability を持つか確認する */
#define capable(cap) (cap_raised(current->cap_effective, (cap)) != 0)

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

/** シグナル共有情報
 * Linux 6.18ではスレッドグループで共有されるシグナル情報
 * 現在は最小限の情報だけを保持し、詳細なキュー管理は未実装
 */
struct signal_struct
{
	atomic_t sigcnt; /* 参照カウント */
};

/** 保留中シグナル
 * 現在は最小限の情報だけを保持し、詳細なキュー管理は未実装
 */
struct sigpending
{
	struct list_head list; /* シグナルキュー */
	uint64_t signal;	   /* 保留中シグナルビットマスク */
};

/** CFS用スケジューリングエンティティ
 * CFS 実装で使用するための構造体。現在は一部のフィールドのみ使用する
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
#define TASK_DEAD 0x0080			/* 終了状態（CPUスケジューリング対象から除外） */
#define TASK_WAKEKILL 0x0100		/* SIGKILLで起床可能 */
#define TASK_WAKING 0x0200			/* 起床処理中 */

/** プロセスの終了状態
 * @brief task_struct->exit_stateの値（__stateとは独立したフィールド）
 * @note Linuxでは__state（スケジューラ用）とexit_state（終了遷移用）を分離している
 * @see Linux 6.18 include/linux/sched.h
 */
#define EXIT_DEAD 0x0010   /* release_task()完了後（完全解放準備済み） */
#define EXIT_ZOMBIE 0x0020 /* 終了済み・親がwait()で回収するまで */
#define EXIT_TRACE (EXIT_ZOMBIE | EXIT_DEAD)

/* タスクフラグ(task_struct->flagsの値) */
#define PF_EXITING 0x00000004			/* 終了中 */
#define PF_KTHREAD 0x00200000			/* カーネルスレッド */
#define PF_WAIT_STOP_PENDING 0x00400000 /* waitpid(WUNTRACED) へ未通知の停止イベントあり */
#define PF_WAIT_CONT_PENDING 0x00800000 /* waitpid(WCONTINUED) へ未通知の再開イベントあり */

/* プロセス名の最大長（Linux 6.18互換） */
#define TASK_COMM_LEN 16

/* カーネルスタックサイズ = 1ページ */
#define THREAD_SIZE PAGE_SIZE

/** コンテキストスイッチ用レジスタ保存領域
 * @brief __switch_to() で callee-saved レジスタを退避・復元する
 *
 * @details sp と task_struct->stack の関係
 * カーネルスタックとは4096バイトの領域であり，
 * その低アドレス側を task_struct->stack が指し，
 * その高アドレス側を task_struct->stack + THREAD_SIZE が指す．
 *
 * sp は「そのプロセスが CPU を手放したときの ESP を退避しておく引き出し」であり，
 * __switch_to()はカーネルモードで呼ばれるため，ESPはカーネルスタックを指す．
 * よって sp は「そのプロセスが最後に使用した自身のカーネルスタック領域内の位置」を指す
 *
 *    stack(低アドレス)          stack + THREAD_SIZE(高アドレス)
 *    ↓                                                    ↓
 *    | ←───*───── カーネルスタック領域(4096バイト) ─────────→ |
 *          ↑sp
 *          (退避されたESPの値)
 *          (そのプロセスが最後に使用した自身のカーネルスタック領域内の位置)
 */
struct thread_struct
{
	unsigned long sp; /* 退避済みのカーネルスタックポインタ */
	unsigned long ip; /* 退避済みのカーネル空間の命令ポインタ（未使用時は 0） */
};

/** スケジューリングポリシー定数
 * @note Linux 由来の値を含むが、kfs では一部 policy のみ実装する。
 */
#define SCHED_NORMAL 0	  /* 通常プロセス用。CFS として実装する */
#define SCHED_FIFO 1	  /* Linux 由来の RT FIFO。現在は未実装 */
#define SCHED_RR 2		  /* Linux 由来の RT RR。現在は未実装 */
#define SCHED_BATCH 3	  /* バッチ処理用。現在は未実装 */
#define SCHED_IDLE 5	  /* 低優先度アイドル用。現在は未実装 */
#define SCHED_DEADLINE 6  /* デッドライン scheduler。現在は未実装 */
#define SCHED_PURE_RR 100 /* kfs 専用の純粋ラウンドロビン */

struct task_struct;

/** スケジューリングクラス
 * @brief 個別スケジューラの実装を呼び出すための操作集合
 */
struct sched_class
{
	void (*init)(void);								/* スケジューラを初期化する */
	void (*enqueue_task)(struct task_struct *task); /* タスクをランキューに追加する */
	void (*dequeue_task)(struct task_struct *task); /* タスクをランキューから削除する */
	int (*task_queued)(struct task_struct *task);	/* タスクがランキューに存在するか確認する */
	struct task_struct *(*pick_next_task)(void);	/* 次に実行するタスクを選択する */
	void (*task_tick)(struct task_struct *task);	/* タスクの1ティック分の時間経過処理を行う */
};

/** プロセス/スレッド記述子
 * @brief プロセス/スレッドの全情報を保持する中核構造体
 */
struct task_struct
{
	/* 状態管理 */
	volatile unsigned int __state; /* プロセス状態（TASK_RUNNING等） */
	void *stack; /* カーネルスタック領域の低アドレス側（stack + THREAD_SIZE が末尾、@see thread_struct） */
	unsigned int flags; /* プロセスフラグ（PF_*） */

	/* メモリ管理 */
	struct mm_struct *mm; /* メモリ記述子 */

	/* プロセスID */
	pid_t pid; /* プロセスID */

	/* プロセスグループ／セッション */
	pid_t pgrp;			/* プロセスグループID */
	pid_t session;		/* セッションID（セッションリーダの PID） */
	size_t tty_console; /* 所属仮想コンソール番号（controlling tty 識別子） */

	/* プロセス階層 */
	struct task_struct *parent; /* 親プロセス */
	struct list_head children;	/* 子プロセスリスト */
	struct list_head sibling;	/* 兄弟プロセスリンク */
	struct list_head tasks;		/* グローバルタスクリストリンク */
	struct list_head run_list;	/* RRランキュー用リンク */

	/* 所有者・権限 */
	kuid_t uid;					/* 実ユーザーID */
	kuid_t euid;				/* 実効ユーザーID（権限チェック用） */
	kernel_cap_t cap_effective; /* 有効なCapability */

	/* シグナル */
	struct signal_struct *signal;		 /* シグナル共有情報 */
	struct sigpending pending;			 /* 保留シグナル */
	struct sigaction sig_actions[_NSIG]; /* プロセスごとのシグナルアクションテーブル */

	/* スケジューリング（CFS用） */
	struct sched_entity se; /* スケジューリングエンティティ（se.run_node, se.vruntimeを使用） */

	/* スケジューリングポリシー */
	unsigned int policy;	 /* スケジューリングポリシー（SCHED_*） */
	int prio;				 /* 動的優先度（0-139、低いほど高優先） */
	int static_prio;		 /* 静的優先度（nice値から算出、SCHED_NORMAL用） */
	int rt_priority;		 /* リアルタイム優先度（1-99、SCHED_RR/FIFO用） */
	unsigned int time_slice; /* 残りタイムスライス（単位：ティック数．非負） */

	/* プロセス名 */
	char comm[TASK_COMM_LEN]; /* プロセス名（最大16バイト） */

	/* コンテキストスイッチ */
	struct thread_struct thread; /* コンテキストスイッチ用レジスタ保存領域 */

	/* プロセス終了情報 */
	int exit_state;	 /* 終了遷移状態（EXIT_ZOMBIE/EXIT_DEAD） */
	int exit_code;	 /* プロセス終了コード（do_wait()で親に返される） */
	int exit_signal; /* 終了時に親に送るシグナル番号（通常SIGCHLD） */

	/* ユーザスタック（do_mmap で確保した場合。exit 時に do_munmap で解放） */
	unsigned long user_stack_vm_start; /* do_mmap が返した仮想アドレス（0=未確保） */
	unsigned long user_stack_vm_len;   /* 確保サイズ（PAGE_SIZE 単位） */
	struct pid *pid_struct;			   /* struct pid のポインタ（alloc_pid の返り値を保存） */

	uint32_t cpu_time_ticks; /* 累積CPU時間（tick単位） */
};

/* 現在実行中のプロセス（kernel/sched/core.c で定義） */
extern struct task_struct *current;

/* fork bomb 防止カウンタ（kernel/fork.c で定義） */
extern int nr_threads; /* 現在のスレッド数 */
/* スケジューラ API（kernel/sched/core.c で実装） */
int schedule(void); /* 1=context switched, 0=no switch */
void scheduler_tick(void);
void wake_up_process(struct task_struct *tsk);
void sched_init(void);

void sched_enqueue_task(struct task_struct *task);
void sched_dequeue_task(struct task_struct *task);
int sched_task_queued(struct task_struct *task);
struct task_struct *sched_pick_next_task(void);
void sched_task_tick(struct task_struct *task);
void cpu_idle_loop(void) __attribute__((weak));
pid_t kernel_thread(void (*fn)(void), const char *name);

/* コンテキストスイッチ（arch/i386/kernel/entry.S で実装） */
void __switch_to(struct task_struct *prev, struct task_struct *next);

/* プロセス管理 API（arch/i386/kernel/process.c で実装） */
void copy_thread(struct task_struct *p, struct task_struct *orig, unsigned long user_eip, unsigned long user_esp);
void switch_mm(struct mm_struct *prev, struct mm_struct *next);

/** task のカーネルスタック内の pt_regs へのポインタを返す
 * @brief スタック最上部（stack + THREAD_SIZE 直下）に pt_regs が配置されている
 */
#define task_pt_regs(task) ((struct pt_regs *)((unsigned long)(task)->stack + THREAD_SIZE) - 1)

/* fork/exec カーネル内部 API（kernel/fork.c で実装） */
void fork_init(void);
pid_t do_fork(unsigned long user_eip, unsigned long arg);

int task_for_each(int (*fn)(struct task_struct *task, void *ctx), void *ctx);

/* exec_fn（kernel/exec.c で実装） */
#include <kfs/exec.h>

#endif /* _KFS_SCHED_H */
