#include <kfs/keyboard.h>
#include <kfs/printk.h>
#include <kfs/string.h>

/** キーボードレイアウトを変更する（loadkeys組み込みコマンド）
 * @param args コマンド引数（レイアウト名）
 * @note テスト用にstaticを外している
 */
void cmd_loadkeys(const char *args)
{
	const char *layout = args;

	/* 先頭の空白をスキップ */
	while (*layout == ' ')
	{
		layout++;
	}

	if (strcmp(layout, "us") == 0 || strcmp(layout, "qwerty") == 0)
	{
		kfs_keyboard_set_layout(KBD_LAYOUT_QWERTY);
		printk("Keyboard layout set to QWERTY (US)\n");
	}
	else if (strcmp(layout, "fr") == 0 || strcmp(layout, "azerty") == 0)
	{
		kfs_keyboard_set_layout(KBD_LAYOUT_AZERTY);
		printk("Keyboard layout set to AZERTY (FR)\n");
	}
	else if (*layout == '\0')
	{
		printk("Usage: loadkeys <us|fr|qwerty|azerty>\n");
	}
	else
	{
		printk("loadkeys: unknown keymap '%s'\n", layout);
		printk("Available keymaps: us, fr, qwerty, azerty\n");
	}
}
