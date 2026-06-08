#include <kfs/neofetch.h>
#include <kfs/printk.h>
#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

/* KFSバージョン */
#define KFS_VERSION "4.0.0"

#define ANSI_RESET "\x1b[0m"
#define ANSI_SKY "\x1b[90m"
#define ANSI_SUN "\x1b[97m"
#define ANSI_RAYS "\x1b[37m"
#define ANSI_HEADER "\x1b[90m"
#define ANSI_LABEL "\x1b[37m"
#define ANSI_VALUE "\x1b[97m"

/** neofetch風のシステム情報画面を表示する
 * @brief
 * - 左側にASCIIアートロゴを表示する
 * - 右側にシステム情報を表示する
 */
void print_neofetch(void)
{
	struct kfs_neofetch_info info;

	if (neofetch_info(&info) < 0)
	{
		memset(&info, 0, sizeof(info));
	}

	printf("\n\n\n\n\n"
		   "%s                  *                     %sroot@kfs%s\n"
		   "%s                    *                   %s---------%s\n"
		   "%s           *     \\ | /                  %sKernel:   %skfs%s\n"
		   "%s                  \\|/                   %sISA:      %si386%s\n"
		   "%s       - -- -- ---   ----- --- -        %sMemory:   %s%lu MiB / %lu MiB%s\n"
		   "%s                  /|\\                     %sFree:   %s%lu MiB%s\n"
		   "%s           *     / | \\     *              %sKernel: %s%lu MiB%s\n"
		   "%s              *         *               %s\n"
		   "%s                  *                     \x1b[44m  \x1b[42m  \x1b[46m  \x1b[41m  \x1b[45m  \x1b[43m  "
		   "\x1b[47m  \x1b[0m%s\n"
		   "\n\n\n\n\n\n\n\n\n\n",
		   ANSI_SKY, ANSI_HEADER, ANSI_RESET, ANSI_SKY, ANSI_HEADER, ANSI_RESET, ANSI_RAYS, ANSI_LABEL, ANSI_VALUE,
		   ANSI_RESET, ANSI_RAYS, ANSI_LABEL, ANSI_VALUE, ANSI_RESET, ANSI_RAYS, ANSI_LABEL, ANSI_VALUE,
		   info.used_mem_mib, info.total_mem_mib, ANSI_RESET, ANSI_RAYS, ANSI_LABEL, ANSI_VALUE, info.free_mem_mib,
		   ANSI_RESET, ANSI_RAYS, ANSI_LABEL, ANSI_VALUE, info.kernel_mem_mib, ANSI_RESET, ANSI_RAYS, ANSI_RESET,
		   ANSI_RAYS, ANSI_RESET);
}

void cmd_neofetch(void *arg)
{
	(void)arg;
	print_neofetch();
}
