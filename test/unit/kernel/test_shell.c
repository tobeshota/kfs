#include "../support/run_in_ring3.h"
#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/keyboard.h>
#include <kfs/list.h>
#include <kfs/sched.h>
#include <kfs/shell.h>
#include <kfs/string.h>
#include <kfs/unistd.h>
#include <kfs/wait.h>

/* シェルの内部関数を外部から呼び出せるように宣言 */
extern int shell_keyboard_handler(char c);
extern void shell_init(void);
extern void cmd_loadkeys(void *args);
extern void cmd_chrt(void *args);
extern void cmd_spin(void *args);
extern void cmd_scx_pure_rr(void *args);
extern void cmd_sched_ext_status(void *args);
extern void cmd_socketpair_test(void *args);
extern int shell_jobs_add(pid_t pid, pid_t pgrp, const char *cmd, int stopped);
extern void shell_jobs_on_wait_event(pid_t pid, int wait_status);
extern void shell_jobs_reset(void);
extern struct task_struct init_task;
extern struct task_struct *current;
extern struct list_head task_list;

static const char *g_loadkeys_args;
static const char *g_chrt_args;

static void run_loadkeys_in_ring3(void)
{
	cmd_loadkeys((void *)g_loadkeys_args);
	exit(0);
}

static void run_loadkeys_cmd(const char *args)
{
	g_loadkeys_args = args;
	run_in_ring3(run_loadkeys_in_ring3);
}

static void run_chrt_in_ring3(void)
{
	cmd_chrt((void *)g_chrt_args);
	exit(0);
}

static void run_chrt_cmd(const char *args)
{
	g_chrt_args = args;
	run_in_ring3(run_chrt_in_ring3);
}

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
	shell_jobs_reset();
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

/* mallocコマンドのテスト */
KFS_TEST(test_shell_execute_malloc_command)
{
	shell_init();

	/* mallocコマンドを実行 */
	shell_keyboard_handler('m');
	shell_keyboard_handler('a');
	shell_keyboard_handler('l');
	shell_keyboard_handler('l');
	shell_keyboard_handler('o');
	shell_keyboard_handler('c');
	shell_keyboard_handler('\n');

	KFS_ASSERT_TRUE(1);
}

/* brkコマンドのテスト */
KFS_TEST(test_shell_execute_brk_command)
{
	shell_init();

	/* brkコマンドを実行 */
	shell_keyboard_handler('b');
	shell_keyboard_handler('r');
	shell_keyboard_handler('k');
	shell_keyboard_handler('\n');

	KFS_ASSERT_TRUE(1);
}

/* vmallocコマンドのテスト */
KFS_TEST(test_shell_execute_vmalloc_command)
{
	shell_init();

	/* vmallocコマンドを実行 */
	shell_keyboard_handler('v');
	shell_keyboard_handler('m');
	shell_keyboard_handler('a');
	shell_keyboard_handler('l');
	shell_keyboard_handler('l');
	shell_keyboard_handler('o');
	shell_keyboard_handler('c');
	shell_keyboard_handler('\n');

	KFS_ASSERT_TRUE(1);
}

/* バッファオーバーフローのテスト */
KFS_TEST(test_shell_buffer_overflow)
{
	shell_init();

	/* CMD_BUFFER_SIZE(256)を超える文字列を入力 */
	for (int i = 0; i < 260; i++)
	{
		shell_keyboard_handler('a');
	}

	/* バッファが満杯になると警告が出てクリアされる */
	KFS_ASSERT_TRUE(1);
}

/* loadkeys us */
KFS_TEST(test_cmd_loadkeys_us)
{
	run_loadkeys_cmd("us");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_QWERTY);
}

/* loadkeys qwerty */
KFS_TEST(test_cmd_loadkeys_qwerty)
{
	run_loadkeys_cmd("qwerty");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_QWERTY);
}

/* loadkeys fr */
KFS_TEST(test_cmd_loadkeys_fr)
{
	run_loadkeys_cmd("fr");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_AZERTY);
}

/* loadkeys azerty */
KFS_TEST(test_cmd_loadkeys_azerty)
{
	run_loadkeys_cmd("azerty");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_AZERTY);
}

/* loadkeys with leading spaces */
KFS_TEST(test_cmd_loadkeys_with_spaces)
{
	run_loadkeys_cmd("   us");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_QWERTY);
}

/* loadkeys without argument */
KFS_TEST(test_cmd_loadkeys_no_argument)
{
	run_loadkeys_cmd("");
	/* Usage メッセージが出力される（エラーなし） */
	KFS_ASSERT_TRUE(1);
}

/* loadkeys with spaces only */
KFS_TEST(test_cmd_loadkeys_spaces_only)
{
	run_loadkeys_cmd("   ");
	/* Usage メッセージが出力される */
	KFS_ASSERT_TRUE(1);
}

/* loadkeys with invalid keymap */
KFS_TEST(test_cmd_loadkeys_invalid)
{
	kbd_layout_t before = kfs_keyboard_get_layout();
	run_loadkeys_cmd("invalid");
	/* レイアウトは変更されない */
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), before);
}

/* 連続してレイアウトを変更 */
KFS_TEST(test_cmd_loadkeys_switch_layouts)
{
	/* US → FR → US */
	run_loadkeys_cmd("us");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_QWERTY);

	run_loadkeys_cmd("fr");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_AZERTY);

	run_loadkeys_cmd("qwerty");
	KFS_ASSERT_EQ(kfs_keyboard_get_layout(), KBD_LAYOUT_QWERTY);
}

/* chrt -o -p 0 <pid> は対象プロセスを SCHED_NORMAL に変更する */
KFS_TEST(test_cmd_chrt_sets_sched_normal)
{
	struct task_struct target = init_task;

	target.pid = 76;
	target.policy = SCHED_EXT;
	INIT_LIST_HEAD(&target.children);
	INIT_LIST_HEAD(&target.sibling);
	INIT_LIST_HEAD(&target.tasks);
	INIT_LIST_HEAD(&target.run_list);
	sched_init_entity(&target);
	list_add_tail(&target.tasks, &task_list);
	run_chrt_cmd("-o -p 0 76");
	KFS_ASSERT_TRUE(target.policy == SCHED_NORMAL);
	list_del(&target.tasks);
}

/* chrt --pure-rr -p 0 <pid> は未対応で、対象プロセスを変更しない */
KFS_TEST(test_cmd_chrt_rejects_pure_rr_option)
{
	struct task_struct target = init_task;

	target.pid = 77;
	target.policy = SCHED_NORMAL;
	INIT_LIST_HEAD(&target.children);
	INIT_LIST_HEAD(&target.sibling);
	INIT_LIST_HEAD(&target.tasks);
	INIT_LIST_HEAD(&target.run_list);
	sched_init_entity(&target);
	list_add_tail(&target.tasks, &task_list);
	run_chrt_cmd("--pure-rr -p 0 77");
	KFS_ASSERT_TRUE(target.policy == SCHED_NORMAL);
	list_del(&target.tasks);
}

/* chrt --ext -p 0 <pid> は対象プロセスを SCHED_EXT に変更する */
KFS_TEST(test_cmd_chrt_sets_sched_ext_for_pid)
{
	struct task_struct target = init_task;

	target.pid = 78;
	target.policy = SCHED_NORMAL;
	INIT_LIST_HEAD(&target.children);
	INIT_LIST_HEAD(&target.sibling);
	INIT_LIST_HEAD(&target.tasks);
	INIT_LIST_HEAD(&target.run_list);
	sched_init_entity(&target);
	list_add_tail(&target.tasks, &task_list);
	run_chrt_cmd("--ext -p 0 78");
	KFS_ASSERT_TRUE(target.policy == SCHED_EXT);
	list_del(&target.tasks);
}

/* chrt -p <pid> は対象processのpolicyを変更せず照会する */
KFS_TEST(test_cmd_chrt_queries_policy_for_pid)
{
	struct task_struct target = init_task;

	target.pid = 79;
	target.policy = SCHED_EXT;
	INIT_LIST_HEAD(&target.children);
	INIT_LIST_HEAD(&target.sibling);
	INIT_LIST_HEAD(&target.tasks);
	INIT_LIST_HEAD(&target.run_list);
	sched_init_entity(&target);
	list_add_tail(&target.tasks, &task_list);
	run_chrt_cmd("-p 79");
	KFS_ASSERT_TRUE(target.policy == SCHED_EXT);
	list_del(&target.tasks);
}

/* spin コマンドは CPU 負荷確認用コマンドとして登録される */
KFS_TEST(test_cmd_spin_registered)
{
	shell_cmd_fn fn;
	const char *args;
	enum shell_cmd_mode mode;

	shell_init();
	KFS_ASSERT_EQ(0, cmd_lookup("spin", &fn, &args, &mode));
	KFS_ASSERT_TRUE(fn == cmd_spin);
	KFS_ASSERT_TRUE(args != 0 && args[0] == '\0');
	KFS_ASSERT_EQ(SHELL_CMD_EXTERNAL, mode);
}

KFS_TEST(test_sched_ext_commands_registered)
{
	shell_cmd_fn fn;
	const char *args;
	enum shell_cmd_mode mode;

	shell_init();
	KFS_ASSERT_EQ(0, cmd_lookup("scx_pure_rr", &fn, &args, &mode));
	KFS_ASSERT_TRUE(fn == cmd_scx_pure_rr);
	KFS_ASSERT_EQ(SHELL_CMD_EXTERNAL, mode);
	KFS_ASSERT_EQ(0, cmd_lookup("sched_ext_status", &fn, &args, &mode));
	KFS_ASSERT_TRUE(fn == cmd_sched_ext_status);
	KFS_ASSERT_EQ(SHELL_CMD_EXTERNAL, mode);
}

/** socketpair_testが外部commandとして登録されることを確かめる */
KFS_TEST(test_socketpair_command_registered)
{
	shell_cmd_fn fn;
	const char *args;
	enum shell_cmd_mode mode;

	shell_init();
	KFS_ASSERT_EQ(0, cmd_lookup("socketpair_test", &fn, &args, &mode));
	KFS_ASSERT_TRUE(fn == cmd_socketpair_test);
	KFS_ASSERT_TRUE(args != 0 && args[0] == '\0');
	KFS_ASSERT_EQ(SHELL_CMD_EXTERNAL, mode);
}

/**
 * test_shell_execute_beep_no_args
 * 検証対象: cmd_beep()
 * 検証項目: "beep" (引数なし) でUsage表示パスをカバー
 */
KFS_TEST(test_shell_execute_beep_no_args)
{
	shell_init();
	/* "beep": *args == '\0' → Usage 表示 → return */
	shell_keyboard_handler('b');
	shell_keyboard_handler('e');
	shell_keyboard_handler('e');
	shell_keyboard_handler('p');
	shell_keyboard_handler('\n');
	KFS_ASSERT_TRUE(1);
}

/**
 * test_shell_execute_beep_zero_freq
 * 検証対象: cmd_beep()
 * 検証項目: "beep 0" で freq <= 0 → pcspkr_stop → return パスをカバー
 */
KFS_TEST(test_shell_execute_beep_zero_freq)
{
	shell_init();
	/* "beep 0": freq=0 → pcspkr_stop() → return */
	shell_keyboard_handler('b');
	shell_keyboard_handler('e');
	shell_keyboard_handler('e');
	shell_keyboard_handler('p');
	shell_keyboard_handler(' ');
	shell_keyboard_handler('0');
	shell_keyboard_handler('\n');
	KFS_ASSERT_TRUE(1);
}

/* Helper: shell_init + type string + Enter */
static void run_shell_cmd(const char *cmd)
{
	shell_init();
	for (size_t i = 0; cmd[i] != '\0'; i++)
	{
		shell_keyboard_handler(cmd[i]);
	}
	shell_keyboard_handler('\n');
}

/* jiffies コマンドのテスト */
KFS_TEST(test_shell_execute_jiffies)
{
	run_shell_cmd("jiffies");
	KFS_ASSERT_TRUE(1);
}

/* glitch コマンドのテスト */
KFS_TEST(test_shell_execute_glitch)
{
	run_shell_cmd("glitch");
	KFS_ASSERT_TRUE(1);
}

/* glitch reset コマンドのテスト */
KFS_TEST(test_shell_execute_glitch_reset)
{
	run_shell_cmd("glitch reset");
	KFS_ASSERT_TRUE(1);
}

/* pginfo コマンドのテスト */
KFS_TEST(test_shell_execute_pginfo)
{
	run_shell_cmd("pginfo");
	KFS_ASSERT_TRUE(1);
}

/* dkstack コマンドのテスト */
KFS_TEST(test_shell_execute_dkstack)
{
	run_shell_cmd("dkstack");
	KFS_ASSERT_TRUE(1);
}

/* neofetch コマンドのテスト */
KFS_TEST(test_shell_execute_neofetch)
{
	run_shell_cmd("neofetch");
	KFS_ASSERT_TRUE(1);
}

/* sleep (引数なし) → Usage 出力して return */
KFS_TEST(test_shell_execute_sleep_no_args)
{
	run_shell_cmd("sleep");
	KFS_ASSERT_TRUE(1);
}

/* sleep 0 → invalid duration → return */
KFS_TEST(test_shell_execute_sleep_zero)
{
	run_shell_cmd("sleep 0");
	KFS_ASSERT_TRUE(1);
}

/* loadkeys us (シェル経由) */
KFS_TEST(test_shell_execute_loadkeys_via_shell)
{
	run_shell_cmd("loadkeys us");
	KFS_ASSERT_TRUE(1);
}

/* loadkeys fr (シェル経由) */
KFS_TEST(test_shell_execute_loadkeys_fr_via_shell)
{
	run_shell_cmd("loadkeys fr");
	KFS_ASSERT_TRUE(1);
}

/* beep negative freq → do_psg_stop path */
KFS_TEST(test_shell_execute_beep_negative)
{
	run_shell_cmd("beep -1");
	KFS_ASSERT_TRUE(1);
}

KFS_TEST(test_shell_jobs_reuse_lowest_id_after_exit)
{
	int id = shell_jobs_add(6, 6, "sleep 30", 0);
	KFS_ASSERT_EQ(1, id);

	shell_jobs_on_wait_event(6, 0);

	id = shell_jobs_add(6, 6, "sleep 30", 0);
	KFS_ASSERT_EQ(1, id);
}

KFS_TEST(test_shell_jobs_reuse_lowest_gap_id)
{
	KFS_ASSERT_EQ(1, shell_jobs_add(10, 10, "sleep 10", 0));
	KFS_ASSERT_EQ(2, shell_jobs_add(11, 11, "sleep 20", 0));
	KFS_ASSERT_EQ(3, shell_jobs_add(12, 12, "sleep 30", 0));

	shell_jobs_on_wait_event(11, 0);

	KFS_ASSERT_EQ(2, shell_jobs_add(13, 13, "sleep 40", 0));
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
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_malloc_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_brk_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_vmalloc_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_buffer_overflow, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_us, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_qwerty, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_fr, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_azerty, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_with_spaces, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_no_argument, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_spaces_only, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_invalid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_loadkeys_switch_layouts, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_chrt_sets_sched_normal, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_chrt_rejects_pure_rr_option, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_chrt_sets_sched_ext_for_pid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_chrt_queries_policy_for_pid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cmd_spin_registered, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_sched_ext_commands_registered, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_socketpair_command_registered, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_beep_no_args, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_beep_zero_freq, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_jiffies, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_glitch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_glitch_reset, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_pginfo, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_dkstack, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_neofetch, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_sleep_no_args, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_sleep_zero, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_loadkeys_via_shell, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_loadkeys_fr_via_shell, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_execute_beep_negative, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_jobs_reuse_lowest_id_after_exit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_shell_jobs_reuse_lowest_gap_id, setup_test, teardown_test),
};

int register_unit_tests_shell(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
