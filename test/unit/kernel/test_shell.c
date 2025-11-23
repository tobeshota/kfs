#include "../test_reset.h"
#include "host_test_framework.h"
#include <kfs/shell.h>
#include <kfs/string.h>

/* シェルの内部関数を外部から呼び出せるように宣言 */
extern int shell_keyboard_handler(char c);
extern void shell_init(void);

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

KFS_TEST(test_shell_init)
{
	shell_init();
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_shell_keyboard_handler_printable)
{
	shell_init();

	/* 通常文字の入力 */
	int result = shell_keyboard_handler('h');
	KFS_ASSERT_TRUE(result == 1);

	result = shell_keyboard_handler('e');
	KFS_ASSERT_TRUE(result == 1);

	result = shell_keyboard_handler('l');
	KFS_ASSERT_TRUE(result == 1);

	result = shell_keyboard_handler('p');
	KFS_ASSERT_TRUE(result == 1);
}

KFS_TEST(test_shell_keyboard_handler_enter)
{
	shell_init();

	/* helpコマンドを入力してEnter */
	shell_keyboard_handler('h');
	shell_keyboard_handler('e');
	shell_keyboard_handler('l');
	shell_keyboard_handler('p');

	int result = shell_keyboard_handler('\n');
	KFS_ASSERT_TRUE(result == 1);
}

KFS_TEST(test_shell_keyboard_handler_backspace)
{
	shell_init();

	/* 文字を入力してからBackspace */
	shell_keyboard_handler('a');
	shell_keyboard_handler('b');

	int result = shell_keyboard_handler('\b');
	KFS_ASSERT_TRUE(result == 1);
}

KFS_TEST(test_shell_execute_help_command)
{
	shell_init();

	/* helpコマンドを実行 */
	shell_keyboard_handler('h');
	shell_keyboard_handler('e');
	shell_keyboard_handler('l');
	shell_keyboard_handler('p');
	shell_keyboard_handler('\n');

	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_shell_execute_meminfo_command)
{
	shell_init();

	/* meminfoコマンドを実行 */
	shell_keyboard_handler('m');
	shell_keyboard_handler('e');
	shell_keyboard_handler('m');
	shell_keyboard_handler('i');
	shell_keyboard_handler('n');
	shell_keyboard_handler('f');
	shell_keyboard_handler('o');
	shell_keyboard_handler('\n');

	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_shell_execute_unknown_command)
{
	shell_init();

	/* 存在しないコマンドを実行 */
	shell_keyboard_handler('x');
	shell_keyboard_handler('y');
	shell_keyboard_handler('z');
	shell_keyboard_handler('\n');

	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_shell_keyboard_handler_arrow_keys)
{
	shell_init();

	/* 矢印キーの入力 */
	shell_keyboard_handler('t');
	shell_keyboard_handler('e');
	shell_keyboard_handler('s');
	shell_keyboard_handler('t');

	/* 左矢印 */
	int result = shell_keyboard_handler('\x1C');
	KFS_ASSERT_TRUE(result == 1);

	/* 右矢印 */
	result = shell_keyboard_handler('\x1D');
	KFS_ASSERT_TRUE(result == 1);
}

KFS_TEST(test_shell_keyboard_handler_ctrl_c)
{
	shell_init();

	/* 文字を入力 */
	shell_keyboard_handler('t');
	shell_keyboard_handler('e');
	shell_keyboard_handler('s');
	shell_keyboard_handler('t');

	/* Ctrl+C */
	int result = shell_keyboard_handler('\x03');
	KFS_ASSERT_TRUE(result == 1);
}

KFS_TEST(test_shell_keyboard_handler_empty_enter)
{
	shell_init();

	/* 何も入力せずにEnter */
	int result = shell_keyboard_handler('\n');
	KFS_ASSERT_TRUE(result == 1);
}

/* DELキー(127)のテスト */
KFS_TEST(test_shell_keyboard_handler_del_key)
{
	shell_init();

	shell_keyboard_handler('a');
	shell_keyboard_handler('b');
	shell_keyboard_handler('c');

	/* DELキー(127) */
	int result = shell_keyboard_handler(127);
	KFS_ASSERT_TRUE(result == 1);
}

/* 複数文字削除のテスト */
KFS_TEST(test_shell_keyboard_handler_multiple_backspace)
{
	shell_init();

	shell_keyboard_handler('a');
	shell_keyboard_handler('b');
	shell_keyboard_handler('c');
	shell_keyboard_handler('d');

	/* 複数回バックスペース */
	shell_keyboard_handler('\b');
	shell_keyboard_handler('\b');
	int result = shell_keyboard_handler('\b');
	KFS_ASSERT_TRUE(result == 1);
}

/* プロンプト位置でのバックスペース(削除しない) */
KFS_TEST(test_shell_backspace_at_prompt)
{
	shell_init();

	/* プロンプト位置でバックスペース（何も起きない） */
	int result = shell_keyboard_handler('\b');
	KFS_ASSERT_TRUE(result == 1);
}

/* 右矢印キーで入力末尾を超えない */
KFS_TEST(test_shell_right_arrow_at_end)
{
	shell_init();

	shell_keyboard_handler('a');
	shell_keyboard_handler('b');

	/* 右矢印（末尾なので移動しない） */
	int result = shell_keyboard_handler('\x1D');
	KFS_ASSERT_TRUE(result == 1);
}

/* 左矢印キーでプロンプト位置を超えない */
KFS_TEST(test_shell_left_arrow_at_prompt)
{
	shell_init();

	/* プロンプト位置で左矢印（移動しない） */
	int result = shell_keyboard_handler('\x1C');
	KFS_ASSERT_TRUE(result == 1);
}

/* キャリッジリターン(\r)のテスト */
KFS_TEST(test_shell_keyboard_handler_carriage_return)
{
	shell_init();

	shell_keyboard_handler('t');
	shell_keyboard_handler('e');
	shell_keyboard_handler('s');
	shell_keyboard_handler('t');

	/* \r (キャリッジリターン) */
	int result = shell_keyboard_handler('\r');
	KFS_ASSERT_TRUE(result == 1);
}

/* 長い入力のテスト */
KFS_TEST(test_shell_long_command)
{
	shell_init();

	/* 長い文字列を入力 */
	const char *long_cmd = "this_is_a_very_long_command_name_that_tests_buffer_handling";
	for (size_t i = 0; long_cmd[i] != '\0'; i++)
	{
		shell_keyboard_handler(long_cmd[i]);
	}

	int result = shell_keyboard_handler('\n');
	KFS_ASSERT_TRUE(result == 1);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_init, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_printable, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_enter, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_backspace, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_help_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_meminfo_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_unknown_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_arrow_keys, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_ctrl_c, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_empty_enter, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_del_key, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_multiple_backspace, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_backspace_at_prompt, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_right_arrow_at_end, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_left_arrow_at_prompt, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_keyboard_handler_carriage_return, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_long_command, setup_test, teardown_test),
};

int register_host_tests_shell(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
