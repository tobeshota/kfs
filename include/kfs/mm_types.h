#ifndef _KFS_MM_TYPES_H
#define _KFS_MM_TYPES_H

#include <asm-i386/pgtable.h>
#include <kfs/list.h>
#include <kfs/rbtree.h>
#include <kfs/stdint.h>

/* 前方宣言 */
struct vm_area_struct;

/** アトミック変数型
 * @param counter カウンタ値
 * @note 単一CPUでは通常のintと同じだが、将来のSMP対応のため定義
 */
typedef struct
{
	int counter;
} atomic_t;

/** プロセスのメモリディスクリプタ
 * @brief プロセスのメモリマップ全体を管理する中核構造体
 * @see Linux 6.18
 * @note pgd_tはasm-i386/pgtable.hで定義（i386ではuint32_tのエイリアス）
 */
struct mm_struct
{
	struct vm_area_struct *mmap; /* VMA（仮想メモリ領域）のリスト */
	pgd_t *pgd; /* ページディレクトリのポインタ（CR3レジスタにロードされる） */

	atomic_t mm_count; /* 参照カウント（fork時のコピー管理用） */

	unsigned long brk;		   /* ヒープ現在位置（sys_brk用） */
	unsigned long start_stack; /* スタック開始アドレス（exec_fn用） */
};

#endif /* _KFS_MM_TYPES_H */
