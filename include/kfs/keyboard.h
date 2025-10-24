#ifndef KFS_KEYBOARD_H
#define KFS_KEYBOARD_H

/* keyboard.h: キーボードドライバ公開インタフェース (drivers/tty/keyboard.c)。機能追加なし。 */

#include <stdint.h>

/** キーボード入力イベントのハンドラ型
 *
 * @param c 入力された文字（通常文字、'\n', '\b', 制御文字など）
 * @return ハンドラが処理した場合は1、キーボードドライバのデフォルト処理を使用する場合は0
 * @example シェルは自身のバッファに文字を追加するためにこのハンドラを使う。
 *          ハンドラが1を返せば、キーボードドライバは何もしない。
 *          ハンドラが0を返せば、キーボードドライバはデフォルトの処理（printk等）を行う。
 */
typedef int (*keyboard_handler_t)(char c);

void kfs_keyboard_init(void);
void kfs_keyboard_reset(void);
void kfs_keyboard_feed_scancode(uint8_t scancode);
void kfs_keyboard_poll(void);

/** カスタムキーボードハンドラを設定する
 * @param handler ハンドラ関数ポインタ（NULLでデフォルト動作に戻す）
 */
void kfs_keyboard_set_handler(keyboard_handler_t handler);

#endif /* KFS_KEYBOARD_H */
