#include <asm-i386/io.h>
#include <kfs/printk.h>

/** 8042キーボードコントローラ経由でリブート
 * @brief 8042キーボードコントローラが持つCPUリセット機能をコマンドポート経由で呼び出す
 * @details
 * I/Oポート0x64番地（8042キーボードコントローラのコマンドポート）に
 * 0xFE（CPUリセットコマンド）を
 * 1バイト書き込む
 *
 * CPU → outb命令 → I/Oポート0x64 → 8042チップ
 *                                     ↓
 *                                0xFE受信
 *                                     ↓
 *                          CPUリセット線をアサート
 *                                     ↓
 *                                CPUリセット
 *
 * @note 8042キーボードコントローラがCPUリセット機能を持つ理由は履歴的経緯．
 *       IBM PC/AT時代，CPUリセットを制御する専用回路がなく，
 *       たまたま8042チップにその機能を持たせた設計だったため．
 *       本来はキーボード制御用のチップだが，副次的にCPUリセット機能も持っている．
 */
__attribute__((weak)) void machine_restart_kbd(void)
{
	outb(0x64, 0xFE);
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
