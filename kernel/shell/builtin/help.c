#include <kfs/printk.h>
#include <kfs/shell.h>

void cmd_help(const char *args, int foreground)
{
	(void)args;
	(void)foreground;
	printk("Built-in commands:\n");
	shell_builtin_list();
}
