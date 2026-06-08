#include <kfs/shell.h>
#include <kfs/unistd.h>

/** システムを停止する（halt組み込みコマンド）
 *
 * @details ring-3 プロセスが halt() syscall を呼ぶことで kernel 側で実行される。
 */
void cmd_halt(void *arg)
{
	(void)arg;
	halt();
}

void cmd_reboot(void *arg)
{
	(void)arg;
	reboot();
}

void cmd_panic(void *arg)
{
	(void)arg;
	trigger_panic();
}
