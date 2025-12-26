#ifndef _KFS_RBTREE_H
#define _KFS_RBTREE_H

#include <kfs/stddef.h>

/* Red-Black Treeのノード */
struct rb_node
{
	/** 親ノードへのポインタと色情報（最下位ビット）
	 * @details ビット配列:
	 *  - 最下位ビット: 色情報（0=黒, 1=赤）
	 *  - 残りのビット: 親ノードへのポインタ
	 */
	unsigned long __rb_parent_color;

	struct rb_node *rb_right; /* 右の子ノード */
	struct rb_node *rb_left;  /* 左の子ノード */
} __attribute__((aligned(sizeof(long))));

/* Red-Black Treeのルート */
struct rb_root
{
	struct rb_node *rb_node; /* ルートノードへのポインタ */
};

/** キャッシュ付きrb_root
 * @note CFS等で最小値へ高速アクセスするため使用
 */
struct rb_root_cached
{
	struct rb_root rb_root;		 /* ツリーのルート */
	struct rb_node *rb_leftmost; /* 最左（最小）ノードへのポインタ */
};

/* rb_rootの初期化マクロ */
#define RB_ROOT                                                                                                        \
	(struct rb_root)                                                                                                   \
	{                                                                                                                  \
		NULL,                                                                                                          \
	}

/* rb_root_cachedの初期化マクロ */
#define RB_ROOT_CACHED                                                                                                 \
	(struct rb_root_cached)                                                                                            \
	{                                                                                                                  \
		{                                                                                                              \
			NULL,                                                                                                      \
		},                                                                                                             \
			NULL                                                                                                       \
	}

/* 色定義 */
#define RB_RED 0
#define RB_BLACK 1

/** ノードの親を取得する
 * @brief 色ビットをマスクして親ポインタを返す
 * @param r 対象ノード
 */
#define rb_parent(r) ((struct rb_node *)((r)->__rb_parent_color & ~3))

/** ノードの色を取得する
 * @param r 対象ノード
 * @return RB_RED (0) または RB_BLACK (1)
 */
#define rb_color(r) ((r)->__rb_parent_color & 1)

/** ノードが赤か判定する
 * @param r 対象ノード
 */
#define rb_is_red(r) (!rb_color(r))

/** ノードが黒か判定する
 * @param r 対象ノード
 */
#define rb_is_black(r) rb_color(r)

/** ノードの親を設定する
 * @param rb 対象ノード
 * @param p 設定する親ノード
 */
static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->__rb_parent_color = rb_color(rb) | (unsigned long)p;
}

/** ノードの色を設定する
 * @param rb 対象ノード
 * @param color 設定する色（RB_RED/RB_BLACK）
 */
static inline void rb_set_color(struct rb_node *rb, int color)
{
	rb->__rb_parent_color = (rb->__rb_parent_color & ~1) | color;
}

/** rb_nodeから含む構造体へのポインタを取得する
 * @param ptr struct rb_nodeへのポインタ
 * @param type 含む構造体の型
 * @param member 構造体内のrb_nodeメンバ名
 */
#define rb_entry(ptr, type, member) container_of(ptr, type, member)

/** キャッシュ付きツリーの最小ノードを取得する
 * @param root キャッシュ付きルート
 */
static inline struct rb_node *rb_first_cached(const struct rb_root_cached *root)
{
	return root->rb_leftmost;
}

struct rb_node *rb_first(const struct rb_root *root);
struct rb_node *rb_next(const struct rb_node *node);
void rb_insert_color(struct rb_node *node, struct rb_root *root);

void rb_erase(struct rb_node *node, struct rb_root *root);

/** 新しいノードをツリーにリンクする
 * @param node リンクする新しいノード
 * @param parent 親ノード
 * @param rb_link 親の左/右ポインタのアドレス
 */
static inline void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **rb_link)
{
	node->__rb_parent_color = (unsigned long)parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node; /* 親の左/右ポインタを更新 */
}

#endif /* _KFS_RBTREE_H */
