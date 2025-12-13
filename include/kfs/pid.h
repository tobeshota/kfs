#ifndef _KFS_PID_H
#define _KFS_PID_H

#include <kfs/list.h>
#include <kfs/stdint.h>

/* 前方宣言 */
struct task_struct;

/* 基本型定義（sched.hからの循環参照を避けるため） */
typedef int pid_t;
typedef int refcount_t;

/* PIDタイプ */
enum pid_type
{
	PIDTYPE_PID,  /* プロセスID */
	PIDTYPE_TGID, /* スレッドグループID（Phase 14: pthread実装で使用） */
	PIDTYPE_MAX	  /* pid_typeの数 */
};

/* ハッシュリストノード */
struct hlist_node
{
	struct hlist_node *next;   /* 次のノード */
	struct hlist_node **pprev; /* 前のノードのnextポインタへのポインタ */
};

/* ハッシュリストヘッド */
struct hlist_head
{
	struct hlist_node *first; /* 最初のノードへのポインタ */
};

/* PID構造体 */
struct pid
{
	/** struct pid への参照数
	 * @brief このPID構造体を指しているポインタの数．
	 * @note 用途により意味が異なる:
	 *       - PIDTYPE_PIDの場合: 通常1（各プロセス/スレッドは独自PID）
	 *       - PIDTYPE_TGID(Phase 14)の場合: スレッドグループ内のスレッド数
	 */
	refcount_t count;

	/** PID namespace階層の深さ
	 * @details PIDのnamespaceを分離させるための仕組み．
	 *          各pidは所属する階層より上位の階層のnamespaceを参照できない．
	 *          コンテナ等で使用される．
	 */
	unsigned int level;

	/* PID namespaceの識別子 */
	unsigned int inum;

	/* PID番号 */
	pid_t nr;

	/** このPIDを持つタスクのハッシュリスト配列
	 * @example
	 * tasks[PIDTYPE_PID]: プロセスリスト
	 * tasks[PIDTYPE_TGID]: スレッドグループリスト（Phase 14で使用）
	 */
	struct hlist_head tasks[PIDTYPE_MAX];
};

/* PID管理関数 */
struct pid *alloc_pid(void);
void put_pid(struct pid *pid);
struct task_struct *find_task_by_pid(pid_t pid);

#endif /* _KFS_PID_H */
