#include <kfs/panic.h>
#include <kfs/printk.h>
#include <kfs/reboot.h>
#include <kfs/shell.h>

/** システムを停止する（halt組み込みコマンド）
 *
 * @details 割り込みを無効化し、汎用レジスタをクリアしてからCPUを停止する。
 *          汎用レジスタをクリアする理由は，hlt後に物理アクセスによる
 *          メモリダンプで機密情報が漏洩することを防ぐため．
 */
void cmd_halt(const char *args, int foreground)
{
	(void)args;
	(void)foreground;
	printk("System halted.\n");

	/* 割り込みを無効化（これ以降は割り込み不可） */
	__asm__ __volatile__("cli");

	/* 汎用レジスタをクリアする（機密情報の漏洩を防ぐため） */
	clear_gp_registers();

	/* CPUを停止する */
	for (;;)
	{
		__asm__ __volatile__("hlt");
	}
}

void cmd_reboot(const char *args, int foreground)
{
	(void)args;
	(void)foreground;
	machine_restart();
}

void cmd_panic(const char *args, int foreground)
{
	(void)args;
	(void)foreground;
	panic("Test panic from shell command");
}
