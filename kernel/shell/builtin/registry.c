#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

struct shell_builtin_entry
{
	const char *name;
	shell_builtin_fn fn;
};

#define MAX_BUILTINS 16

static struct shell_builtin_entry builtin_table[MAX_BUILTINS];
static size_t builtin_count;

void builtin_register(const char *name, shell_builtin_fn fn)
{
	if (builtin_count >= MAX_BUILTINS)
	{
		return;
	}
	builtin_table[builtin_count].name = name;
	builtin_table[builtin_count].fn = fn;
	builtin_count++;
}

int lookup_builtin(const char *cmd, shell_builtin_fn *fn, const char **args)
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
		return 1;
	}
	return 0;
}

void shell_builtin_list(void)
{
	for (size_t i = 0; i < builtin_count; i++)
	{
		printf("  %s\n", builtin_table[i].name);
	}
}

void shell_builtins_init(void)
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
	extern void cmd_sleep(void *args);
	extern void cmd_daiku(void *args);
	extern void cmd_help(void *args);

	builtin_register("halt", cmd_halt);
	builtin_register("reboot", cmd_reboot);
	builtin_register("panic", cmd_panic);
	builtin_register("loadkeys", cmd_loadkeys);
	builtin_register("ps", cmd_ps);
	builtin_register("kill", cmd_kill);
	builtin_register("piano", cmd_piano);
	builtin_register("neofetch", cmd_neofetch);
	builtin_register("sched", cmd_sched);
	builtin_register("beep", cmd_beep);
	builtin_register("sleep", cmd_sleep);
	builtin_register("daiku", cmd_daiku);
	builtin_register("help", cmd_help);
}
