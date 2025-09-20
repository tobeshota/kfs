#ifndef LINUX_KEYBOARD_H
#define LINUX_KEYBOARD_H

/* keyboard.h: キーボードドライバ公開インタフェース (drivers/tty/keyboard.c)。機能追加なし。 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void kfs_keyboard_init(void);
void kfs_keyboard_reset(void);
void kfs_keyboard_feed_scancode(uint8_t scancode);
void kfs_keyboard_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_KEYBOARD_H */
