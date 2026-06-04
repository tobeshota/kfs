#include <kfs/printk.h>
#include <kfs/shell.h>
#include <kfs/string.h>

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
		printk("  %s\n", builtin_table[i].name);
	}
}

void shell_builtins_init(void)
{
	builtin_count = 0;
	extern void cmd_halt(const char *args, int foreground);
	extern void cmd_reboot(const char *args, int foreground);
	extern void cmd_panic(const char *args, int foreground);
	extern void cmd_loadkeys(const char *args, int foreground);
	extern void cmd_ps(const char *args, int foreground);
	extern void cmd_kill(const char *args, int foreground);
	extern void cmd_piano(const char *args, int foreground);
	extern void cmd_neofetch(const char *args, int foreground);
	extern void cmd_sched(const char *args, int foreground);
	extern void cmd_beep(const char *args, int foreground);
	extern void cmd_sleep(const char *args, int foreground);
	extern void cmd_daiku(const char *args, int foreground);
	extern void cmd_help(const char *args, int foreground);
	extern void sched_ring3_main(void);

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
