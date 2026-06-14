#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

struct shell_builtin_entry
{
	const char *name;		  /* コマンド名 */
	shell_cmd_fn fn;		  /* コマンド関数 */
	enum shell_cmd_mode mode; /* コマンドモード */
};

#define MAX_BUILTINS 24

static struct shell_builtin_entry builtin_table[MAX_BUILTINS];
static size_t builtin_count;

/** コマンドを登録する
 * @param name コマンド名
 * @param fn コマンド関数
 * @param mode コマンドモード
 */
void cmd_register(const char *name, shell_cmd_fn fn, enum shell_cmd_mode mode)
{
	if (builtin_count >= MAX_BUILTINS)
	{
		return;
	}
	builtin_table[builtin_count].name = name;
	builtin_table[builtin_count].fn = fn;
	builtin_table[builtin_count].mode = mode;
	builtin_count++;
}

/** コマンド名cmdからコマンド情報を検索する
 * @brief コマンド名cmdからfn，args，modeを検索する
 * @param cmd コマンド名
 * @param fn コマンド関数のポインタ
 * @param args コマンド引数のポインタ
 * @param mode コマンドモードのポインタ
 * @return 0 成功, -1 失敗
 */
int cmd_lookup(const char *cmd, shell_cmd_fn *fn, const char **args, enum shell_cmd_mode *mode)
{
	for (size_t i = 0; i < builtin_count; i++)
	{
		size_t name_len = strlen(builtin_table[i].name);

		if (strncmp(cmd, builtin_table[i].name, name_len) != 0)
		{
			continue;
		}
		if (cmd[name_len] != '\0' && cmd[name_len] != ' ')
		{
			continue;
		}
		*fn = builtin_table[i].fn;
		*args = cmd + name_len;
		*mode = builtin_table[i].mode;
		return 0;
	}
	return -1;
}

/** 指定モードのコマンド一覧を表示する
 * @param mode コマンドモード
 * - SHELL_CMD_BUILTIN:  組み込みコマンドのみ表示
 * - SHELL_CMD_EXTERNAL: 外部コマンドのみ表示
 */
void cmd_list(enum shell_cmd_mode mode)
{
	for (size_t i = 0; i < builtin_count; i++)
	{
		if (builtin_table[i].mode != mode)
		{
			continue;
		}
		printf("  %s\n", builtin_table[i].name);
	}
}

/* コマンドレジストリを初期化する */
void cmd_registry_init(void)
{
	builtin_count = 0;
	extern void cmd_halt(void *args);
	extern void cmd_reboot(void *args);
	extern void cmd_panic(void *args);
	extern void cmd_loadkeys(void *args);
	extern void cmd_ps(void *args);
	extern void cmd_kill(void *args);
	extern void cmd_piano(void *args);
	extern void cmd_neofetch(void *args);
	extern void cmd_sched(void *args);
	extern void cmd_beep(void *args);
	extern void cmd_chrt(void *args);
	extern void cmd_sleep(void *args);
	extern void cmd_spin(void *args);
	extern void cmd_daiku(void *args);
	extern void cmd_help(void *args);
	extern void cmd_jobs(void *args);
	extern void cmd_fg(void *args);
	extern void cmd_bg(void *args);

	cmd_register("halt", cmd_halt, SHELL_CMD_EXTERNAL);
	cmd_register("reboot", cmd_reboot, SHELL_CMD_EXTERNAL);
	cmd_register("panic", cmd_panic, SHELL_CMD_EXTERNAL);
	cmd_register("loadkeys", cmd_loadkeys, SHELL_CMD_EXTERNAL);
	cmd_register("ps", cmd_ps, SHELL_CMD_EXTERNAL);
	cmd_register("kill", cmd_kill, SHELL_CMD_EXTERNAL);
	cmd_register("piano", cmd_piano, SHELL_CMD_EXTERNAL);
	cmd_register("neofetch", cmd_neofetch, SHELL_CMD_EXTERNAL);
	cmd_register("sched", cmd_sched, SHELL_CMD_EXTERNAL);
	cmd_register("beep", cmd_beep, SHELL_CMD_EXTERNAL);
	cmd_register("chrt", cmd_chrt, SHELL_CMD_EXTERNAL);
	cmd_register("sleep", cmd_sleep, SHELL_CMD_EXTERNAL);
	cmd_register("spin", cmd_spin, SHELL_CMD_EXTERNAL);
	cmd_register("daiku", cmd_daiku, SHELL_CMD_EXTERNAL);
	cmd_register("help", cmd_help, SHELL_CMD_EXTERNAL);
	cmd_register("jobs", cmd_jobs, SHELL_CMD_BUILTIN);
	cmd_register("fg", cmd_fg, SHELL_CMD_BUILTIN);
	cmd_register("bg", cmd_bg, SHELL_CMD_BUILTIN);
}
