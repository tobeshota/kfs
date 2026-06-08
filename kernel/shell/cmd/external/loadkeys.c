#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/string.h>
#include <kfs/unistd.h>

/** キーボードレイアウトを変更する（loadkeys組み込みコマンド）
 * @param args コマンド引数（レイアウト名）
 * @note テスト用にstaticを外している
 */
void cmd_loadkeys(void *arg)
{
	const char *layout = arg;

	/* 先頭の空白をスキップ */
	while (*layout == ' ')
	{
		layout++;
	}

	if (strcmp(layout, "us") == 0 || strcmp(layout, "qwerty") == 0)
	{
		kbd_set_layout(KBD_LAYOUT_QWERTY);
	}
	else if (strcmp(layout, "fr") == 0 || strcmp(layout, "azerty") == 0)
	{
		kbd_set_layout(KBD_LAYOUT_AZERTY);
	}
	else if (*layout == '\0')
	{
		printf("Usage: loadkeys <us|fr|qwerty|azerty>\n");
	}
	else
	{
		printf("loadkeys: unknown keymap\nAvailable keymaps: us, fr, qwerty, azerty\n");
	}
}
