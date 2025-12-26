#include <kfs/rbtree.h>
#include <kfs/stddef.h>

/** ツリーの最小ノードを取得する
 * @brief ツリーを左方向に辿って最小値を見つける
 * @param root ツリーのルート
 * @return 最左（最小）ノード，ツリーが空ならNULL
 */
struct rb_node *rb_first(const struct rb_root *root)
{
	struct rb_node *n;

	n = root->rb_node;
	if (!n)
	{
		return NULL;
	}

	/* 左端まで辿る */
	while (n->rb_left)
	{
		n = n->rb_left;
	}

	return n;
}

/** ノードの次（より大きい）ノードを取得する
 * @param node 現在のノード
 * @return 次のノード，nodeが最大ならNULL
 */
struct rb_node *rb_next(const struct rb_node *node)
{
	struct rb_node *parent;

	if (!node)
	{
		return NULL;
	}

	/* 右の子がある場合、その部分木の最小値 */
	if (node->rb_right)
	{
		node = node->rb_right;
		while (node->rb_left)
		{
			node = node->rb_left;
		}
		return (struct rb_node *)node;
	}

	/* 右の子がない場合、親を遡って探す */
	while ((parent = rb_parent(node)) && node == parent->rb_right)
	{
		node = parent;
	}

	return parent;
}

/** ノード挿入後の色を調整する
 * @param node 挿入したノード
 * @param root ツリーのルート
 *
 * Phase 7で完全実装予定
 * 今はノードを黒に設定するだけ
 */
void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
	(void)root; /* Phase 7で使用 */
	/* 簡易実装：単純に黒に設定 */
	rb_set_color(node, RB_BLACK);

	/* Phase 7で完全なRed-Black Tree調整を実装 */
}

/** ツリーからノードを削除する
 * @param node 削除するノード
 * @param root ツリーのルート
 *
 * Phase 7で完全実装予定
 */
void rb_erase(struct rb_node *node, struct rb_root *root)
{
	(void)root; /* Phase 7で使用 */
	/* Phase 7で実装 */
	/* 今はリンク解除のみ */
	if (node->rb_left && node->rb_right)
	{
		/* 両方の子がある場合は複雑なので未実装 */
		return;
	}

	/* 子が1つ以下の場合の簡易削除 */
	struct rb_node *child = node->rb_left ? node->rb_left : node->rb_right;
	struct rb_node *parent = rb_parent(node);

	if (child)
	{
		rb_set_parent(child, parent);
	}

	if (parent)
	{
		if (parent->rb_left == node)
		{
			parent->rb_left = child;
		}
		else
		{
			parent->rb_right = child;
		}
	}
	else
	{
		root->rb_node = child;
	}
}
