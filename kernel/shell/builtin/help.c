#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/unistd.h>

void cmd_help(void *arg)
{
	(void)arg;
	printf("Built-in commands:\n");
	shell_builtin_list();
}
