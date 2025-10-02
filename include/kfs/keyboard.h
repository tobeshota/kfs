#ifndef KFS_KEYBOARD_H
#define KFS_KEYBOARD_H

/* keyboard.h: キーボードドライバ公開インタフェース (drivers/tty/keyboard.c)。機能追加なし。 */

#include <stdint.h>

void kfs_keyboard_init(void);
void kfs_keyboard_reset(void);
void kfs_keyboard_feed_scancode(uint8_t scancode);
void kfs_keyboard_poll(void);

#endif /* KFS_KEYBOARD_H */
