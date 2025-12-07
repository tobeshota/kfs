#ifndef KFS_KEYBOARD_H
#define KFS_KEYBOARD_H

#include <kfs/stdint.h>

/* キーボードIRQ番号 */
#define KEYBOARD_IRQ 1

/* キーボードレイアウト種別 */
typedef enum
{
	KBD_LAYOUT_QWERTY = 0, /* US QWERTY配列 */
	KBD_LAYOUT_AZERTY = 1, /* フランス語AZERTY配列 */
} kbd_layout_t;

/** キーボード入力イベントのハンドラ型
 * @param c 入力された文字（通常文字、'\n', '\b', 制御文字など）
 * @return ハンドラが処理した場合は1、デフォルト処理を使用する場合は0
 */
typedef int (*keyboard_handler_t)(char c);

void kfs_keyboard_init(void);
void kfs_keyboard_reset(void);
void kfs_keyboard_feed_scancode(uint8_t scancode);
void kfs_keyboard_set_handler(keyboard_handler_t handler);
void kfs_keyboard_set_layout(kbd_layout_t layout);
kbd_layout_t kfs_keyboard_get_layout(void);

#endif /* KFS_KEYBOARD_H */
