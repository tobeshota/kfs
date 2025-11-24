#include <asm-i386/io.h>
#include <kfs/printk.h>

/** 8042キーボードコントローラ経由でリブート
 * @details ポート0x64に0xFEコマンドを送信してCPUリセットを実行する．
 * @note この関数はテスト可能にするため非staticだが
 *       公開ヘッダーでは宣言されていない（内部使用のみ）．
 * @see Linux 2.6.11 kbd_write_command_w(0xFE)
 */
__attribute__((weak)) void machine_restart_kbd(void)
{
	outb(0xFE, 0x64);
}

/** システムを再起動する
 * @note この関数は戻らない
 * @see Linux 2.6.11 machine_restart()
 */
void machine_restart(void)
{
	printk(KERN_EMERG "Rebooting...\n");

	/* 8042キーボードコントローラリセットを試行 */
	machine_restart_kbd();

	/* リセットが失敗した場合はCPUを停止 */
	for (;;)
	{
		asm volatile("cli; hlt");
	}
}
