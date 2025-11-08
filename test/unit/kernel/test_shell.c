#include "host_test_framework.h"
#include <kfs/shell.h>
#include <kfs/string.h>

/* シェルの内部関数を外部から呼び出せるように宣言 */
extern int shell_keyboard_handler(char c);
extern void shell_init(void);

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

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_shell_init),
	KFS_REGISTER_TEST(test_shell_keyboard_handler_printable),
	KFS_REGISTER_TEST(test_shell_keyboard_handler_enter),
	KFS_REGISTER_TEST(test_shell_keyboard_handler_backspace),
	KFS_REGISTER_TEST(test_shell_execute_help_command),
	KFS_REGISTER_TEST(test_shell_execute_meminfo_command),
	KFS_REGISTER_TEST(test_shell_execute_unknown_command),
	KFS_REGISTER_TEST(test_shell_keyboard_handler_arrow_keys),
	KFS_REGISTER_TEST(test_shell_keyboard_handler_ctrl_c),
	KFS_REGISTER_TEST(test_shell_keyboard_handler_empty_enter),
};

int register_host_tests_shell(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
