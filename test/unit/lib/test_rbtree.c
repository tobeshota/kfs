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

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_node_structure, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_root_initialization, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_link_node, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_color_operations, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_first, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_rb_parent_operations, setup_test, teardown_test),
};

int register_unit_tests_rbtree(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
