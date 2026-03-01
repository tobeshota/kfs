#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/rbtree.h>

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 必要なら後処理（現在は空） */
}

/**
 * test_rb_node_structure - rb_node構造体のサイズと配置テスト
 *
 * rb_nodeが期待通りのサイズとアライメントを持つか確認
 */
static void test_rb_node_structure(void)
{
	struct rb_node node;
	size_t size = sizeof(struct rb_node);

	/* rb_nodeは3ポインタ分のサイズ */
	KFS_ASSERT_TRUE(size == 3 * sizeof(unsigned long));

	/* アライメント確認 */
	KFS_ASSERT_TRUE(((unsigned long)&node) % sizeof(long) == 0);

	printk("rb_node size: %u bytes\n", size);
}

/**
 * test_rb_root_initialization - rb_root初期化のテスト
 *
 * RB_ROOT/RB_ROOT_CACHEDマクロが正しく動作するか確認
 */
static void test_rb_root_initialization(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_root_cached cached = RB_ROOT_CACHED;

	/* 空のツリー */
	KFS_ASSERT_TRUE(root.rb_node == NULL);
	KFS_ASSERT_TRUE(cached.rb_root.rb_node == NULL);
	KFS_ASSERT_TRUE(cached.rb_leftmost == NULL);

	printk("rb_root initialization test passed\n");
}

/**
 * test_rb_link_node - rb_link_node()のテスト
 *
 * ノードのリンクが正しく行われるか確認
 */
static void test_rb_link_node(void)
{
	struct rb_node parent, child;
	struct rb_node **link;

	/* 親ノードをルートに設定 */
	parent.__rb_parent_color = 0;
	parent.rb_left = NULL;
	parent.rb_right = NULL;

	/* 子ノードを左に追加 */
	link = &parent.rb_left;
	rb_link_node(&child, &parent, link);

	/* リンク確認 */
	KFS_ASSERT_TRUE(parent.rb_left == &child);
	KFS_ASSERT_TRUE(rb_parent(&child) == &parent);
	KFS_ASSERT_TRUE(child.rb_left == NULL);
	KFS_ASSERT_TRUE(child.rb_right == NULL);

	printk("rb_link_node test passed\n");
}

/**
 * test_rb_color_operations - 色操作のテスト
 *
 * rb_set_color/rb_color/rb_is_red/rb_is_blackが正しく動作するか確認
 */
static void test_rb_color_operations(void)
{
	struct rb_node node;

	/* ノードを初期化 */
	node.__rb_parent_color = 0;
	node.rb_left = NULL;
	node.rb_right = NULL;

	/* 赤に設定 */
	rb_set_color(&node, RB_RED);
	KFS_ASSERT_TRUE(rb_color(&node) == RB_RED);
	KFS_ASSERT_TRUE(rb_is_red(&node));
	KFS_ASSERT_TRUE(!rb_is_black(&node));

	/* 黒に設定 */
	rb_set_color(&node, RB_BLACK);
	KFS_ASSERT_TRUE(rb_color(&node) == RB_BLACK);
	KFS_ASSERT_TRUE(rb_is_black(&node));
	KFS_ASSERT_TRUE(!rb_is_red(&node));

	printk("rb_color operations test passed\n");
}

/**
 * test_rb_first - rb_first()のテスト
 *
 * 空のツリーと単一ノードのツリーでrb_first()が正しく動作するか確認
 */
static void test_rb_first(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node *first;
	struct rb_node node;

	/* 空のツリー */
	first = rb_first(&root);
	KFS_ASSERT_TRUE(first == NULL);

	/* 単一ノード */
	node.__rb_parent_color = 0;
	node.rb_left = NULL;
	node.rb_right = NULL;
	root.rb_node = &node;

	first = rb_first(&root);
	KFS_ASSERT_TRUE(first == &node);

	printk("rb_first test passed\n");
}

/**
 * test_rb_parent_operations - 親ノード操作のテスト
 *
 * rb_parent/rb_set_parentが正しく動作するか確認
 */
static void test_rb_parent_operations(void)
{
	struct rb_node parent, child;

	/* 初期化 */
	parent.__rb_parent_color = 0;
	child.__rb_parent_color = 0;

	/* 親を設定 */
	rb_set_parent(&child, &parent);
	KFS_ASSERT_TRUE(rb_parent(&child) == &parent);

	/* 色は保持される */
	rb_set_color(&child, RB_RED);
	rb_set_parent(&child, &parent);
	KFS_ASSERT_TRUE(rb_is_red(&child));

	printk("rb_parent operations test passed\n");
}

/** 3ノードツリーで rb_next() が正しい次ノードを返すか確かめる
 *        root_node
 *       /          \
 *  left_node    right_node
 *
 * rb_next(left_node)  → root_node
 * rb_next(root_node)  → right_node
 * rb_next(right_node) → NULL
 */
static void test_rb_next(void)
{
	struct rb_node *result;
	struct rb_node root_node, left_node, right_node;

	/* NULLの場合 */
	result = rb_next(NULL);
	KFS_ASSERT_TRUE(result == NULL);

	/* 単独ノード（親なし、右の子なし）→ NULL */
	root_node.__rb_parent_color = 0;
	root_node.rb_left = NULL;
	root_node.rb_right = NULL;
	result = rb_next(&root_node);
	KFS_ASSERT_TRUE(result == NULL);

	/* 3ノードツリーを構築 */
	root_node.__rb_parent_color = 0; /* ルート: 親なし */
	root_node.rb_left = &left_node;
	root_node.rb_right = &right_node;

	left_node.__rb_parent_color = (unsigned long)&root_node; /* 親=root */
	left_node.rb_left = NULL;
	left_node.rb_right = NULL;

	right_node.__rb_parent_color = (unsigned long)&root_node; /* 親=root */
	right_node.rb_left = NULL;
	right_node.rb_right = NULL;

	/* 左の子の次は親 */
	result = rb_next(&left_node);
	KFS_ASSERT_TRUE(result == &root_node);

	/* 右の子がある場合、右の部分木の最小値 */
	result = rb_next(&root_node);
	KFS_ASSERT_TRUE(result == &right_node);

	/* ツリーの最大値の次は NULL */
	result = rb_next(&right_node);
	KFS_ASSERT_TRUE(result == NULL);

	printk("rb_next test passed\n");
}

/* rb_insert_color() がノードを黒に設定するか確かめる */
static void test_rb_insert_color(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node node;

	node.__rb_parent_color = 0;
	node.rb_left = NULL;
	node.rb_right = NULL;

	/* 赤ノードを挿入 → 黒になる（現在の簡易実装） */
	rb_set_color(&node, RB_RED);
	KFS_ASSERT_TRUE(rb_is_red(&node));

	rb_insert_color(&node, &root);
	KFS_ASSERT_TRUE(rb_is_black(&node));

	printk("rb_insert_color test passed\n");
}
/** rb_erase テスト: 両方の子があるノードは early return */
static void test_rb_erase_both_children_early_return(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node parent, left, right;

	parent.__rb_parent_color = 0;
	parent.rb_left = &left;
	parent.rb_right = &right;
	left.__rb_parent_color = (unsigned long)&parent;
	left.rb_left = NULL;
	left.rb_right = NULL;
	right.__rb_parent_color = (unsigned long)&parent;
	right.rb_left = NULL;
	right.rb_right = NULL;
	root.rb_node = &parent;

	rb_erase(&parent, &root);
	/* 両子ありは early return -> root.rb_node 変わらず */
	KFS_ASSERT_TRUE(root.rb_node == &parent);
}

/** rb_erase テスト: 葉ノードを親の左から削除 */
static void test_rb_erase_leaf_left_of_parent(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node parent, leaf;

	parent.__rb_parent_color = 0;
	parent.rb_left = &leaf;
	parent.rb_right = NULL;
	leaf.__rb_parent_color = (unsigned long)&parent;
	leaf.rb_left = NULL;
	leaf.rb_right = NULL;
	root.rb_node = &parent;

	rb_erase(&leaf, &root);
	KFS_ASSERT_TRUE(parent.rb_left == NULL);
}

/** rb_erase テスト: 葉ノードを親の右から削除 */
static void test_rb_erase_leaf_right_of_parent(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node parent, leaf;

	parent.__rb_parent_color = 0;
	parent.rb_left = NULL;
	parent.rb_right = &leaf;
	leaf.__rb_parent_color = (unsigned long)&parent;
	leaf.rb_left = NULL;
	leaf.rb_right = NULL;
	root.rb_node = &parent;

	rb_erase(&leaf, &root);
	KFS_ASSERT_TRUE(parent.rb_right == NULL);
}

/** rb_erase テスト: 子なし・ルートノードを削除 */
static void test_rb_erase_root_no_children(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node node;

	node.__rb_parent_color = 0;
	node.rb_left = NULL;
	node.rb_right = NULL;
	root.rb_node = &node;

	rb_erase(&node, &root);
	KFS_ASSERT_TRUE(root.rb_node == NULL);
}

/** rb_erase テスト: 左子あり・ルートノードを削除 */
static void test_rb_erase_root_with_left_child(void)
{
	struct rb_root root = RB_ROOT;
	struct rb_node node, left_child;

	node.__rb_parent_color = 0;
	node.rb_left = &left_child;
	node.rb_right = NULL;
	left_child.__rb_parent_color = (unsigned long)&node;
	left_child.rb_left = NULL;
	left_child.rb_right = NULL;
	root.rb_node = &node;

	rb_erase(&node, &root);
	KFS_ASSERT_TRUE(root.rb_node == &left_child);
}
static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_node_structure, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_root_initialization, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_link_node, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_color_operations, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_first, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_parent_operations, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_next, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_insert_color, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_erase_both_children_early_return, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_erase_leaf_left_of_parent, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_erase_leaf_right_of_parent, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_erase_root_no_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_erase_root_with_left_child, setup_test, teardown_test),
};

int register_unit_tests_rbtree(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
