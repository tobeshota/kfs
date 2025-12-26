#ifndef _KFS_LIST_H
#define _KFS_LIST_H

#include <kfs/stddef.h>

/* 双方向リストのノード */
struct list_head
{
	struct list_head *next; /* 次のノードへのポインタ */
	struct list_head *prev; /* 前のノードへのポインタ */
};

/** 空のリストを作る
 * @param name リストヘッド変数名
 * @brief 双方向リストのnextとprevが自分自身を指すように初期化する
 */
#define LIST_HEAD_INIT(name)                                                                                           \
	{                                                                                                                  \
		&(name), &(name)                                                                                               \
	}

/** 空のリストヘッドを定義・初期化する
 * @param name 定義する変数名
 */
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

/** リストヘッドを実行時に初期化する
 * @param list 初期化するリストヘッド
 * @brief 自分自身を指すようにしてリストを空にする
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

/** 2つのノード間に新しいノードを挿入する
 * @param new 挿入する新しいノード
 * @param prev 前のノード
 * @param next 次のノード
 */
static inline void __list_add(struct list_head *new, struct list_head *prev, struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/** リストの先頭に新しいノードを追加する
 * @param new 追加する新しいノード
 * @param head リストヘッド
 * @note headの直後に挿入する（LIFO）
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/** リストの末尾に新しいノードを追加する
 * @param new 追加する新しいノード
 * @param head リストヘッド
 * @note headの直前に挿入する（FIFO）
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/** 2つのノード間のノードを削除する
 * @param prev 前のノード
 * @param next 次のノード
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/**　リストからノードを削除する
 * @brief ノードをリストから切り離し、nextとprevをNULLに設定する
 * @param entry 削除するノード
 */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

/** リストが空かチェックする
 * @param head チェックするリストヘッド
 * @return リストが空なら1、要素があれば0
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/** メンバのアドレスから構造体のアドレスを取得する
 * @param ptr メンバへのポインタ
 * @param type メンバを含む構造体の型
 * @param member 構造体内のメンバ名
 *
 * @example foo構造体のメンバx.bからfoo構造体へのアドレスを取得する
 * ```c
 * struct foo {
 *     int a;
 *     char b;
 * };
 *
 * struct foo x;
 *
 * // メンバのアドレス&x.bからfoo構造体へのポインタを取得する
 * struct foo *px = container_of(&x.b, struct foo, b);
 * ```
 *
 * @details メンバのアドレスから構造体のアドレスを取得する方法
 * 1. ptrがtype構造体のmemberメンバへのポインタであることを型チェックにより調べる
 *    const typeof(((type *)0)->member) *__mptr = (ptr);
 *   a. 型計算用に「アドレス0にあるtype構造体」という仮想の構造体を作る
 *      (type *)0
 *   b. 「アドレス0にあるtype構造体」のmemberメンバの型を取得する
 * 	    typeof(((type *)0)->member)
 *   c. 「アドレス0にあるtype構造体」のメンバmemberの型の変数*__mptrを宣言する
 *      // *mptrの型はtype->memberの型になる
 *      const typeof(((type *)0)->member) *__mptr
 *   d. ptrを__mptrに代入し、ptrの型がtype->memberの型と一致するかチェックする
 *      const typeof(((type *)0)->member) *__mptr = (ptr);
 *  2. ptrからmemberのオフセットを引いて構造体の先頭アドレスを計算する
 *      (char *)__mptr - offsetof(type, member)
 *
 * @see linux-2.6.11: include/linux/kernel.h
 */
#define container_of(ptr, type, member)                                                                                \
	({                                                                                                                 \
		const typeof(((type *)0)->member) *__mptr = (ptr); /* 型安全性チェック */                              \
		(type *)((char *)__mptr - offsetof(type, member)); /* 構造体先頭アドレスを計算 */                  \
	})

/** リストノードから含む構造体へのポインタを取得する
 * @param ptr struct list_headへのポインタ
 * @param type 含む構造体の型
 * @param member 構造体内のlist_headメンバ名
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/** リストの各要素に対してループ処理を行う
 * @param pos 反復用の構造体ポインタ
 * @param head リストヘッド
 * @param member 構造体内のlist_headメンバ名
 *
 * @example 子プロセスをすべて表示する
 * ```c
 * struct task_struct *child;
 * list_for_each_entry(child, &parent->children, sibling) {
 *     printk("Child Process: PID=%d\n", child->pid);
 * }
 * ```
 */
#define list_for_each_entry(pos, head, member)                                                                         \
	for (pos = list_entry((head)->next, typeof(*pos), member); &pos->member != (head);                                 \
		 pos = list_entry(pos->member.next, typeof(*pos), member))

#endif /* _KFS_LIST_H */
