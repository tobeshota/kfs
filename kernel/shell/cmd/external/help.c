#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/unistd.h>

void cmd_help(void *arg)
{
	(void)arg;
	printf("Built-in commands:\n");
	cmd_list(SHELL_CMD_BUILTIN);
	printf("External commands:\n");
	cmd_list(SHELL_CMD_EXTERNAL);
}
