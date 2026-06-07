#ifndef KFS_KEYBOARD_H
#define KFS_KEYBOARD_H

#include <kfs/stddef.h>
#include <kfs/stdint.h>

/* キーボードIRQ番号 */
#define KEYBOARD_IRQ 1

/* PS/2 スキャンコードは 7 ビット(0x00-0x7F)なのでテーブルサイズは 128 */
#define KEYBOARD_SCANCODE_MAX 128

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

/** RAW スキャンコードハンドラ型
 * @param code    キーコード（scancode & 0x7F）
 * @param release 0=押下, 1=解放
 * @return 1=処理済み（以降の通常処理をスキップ）, 0=通常処理に委譲
 */
typedef int (*keyboard_raw_handler_t)(uint8_t code, int release);

struct kfs_keyboard_raw_event
{
	uint8_t code;
	uint8_t release;
};

void kfs_keyboard_init(void);
void kfs_keyboard_reset(void);
void kfs_keyboard_feed_scancode(uint8_t scancode);
void kfs_keyboard_set_handler(keyboard_handler_t handler);
void kfs_keyboard_set_raw_handler(keyboard_raw_handler_t handler);
void kfs_keyboard_set_layout(kbd_layout_t layout);
kbd_layout_t kfs_keyboard_get_layout(void);
long kfs_keyboard_read_line(char *buf, unsigned int size);
long kfs_keyboard_read_line_for_console(size_t console_index, char *buf, unsigned int size);
long kfs_keyboard_read_event(struct kfs_keyboard_raw_event *event);
void kfs_keyboard_clear_events(void);
void kfs_keyboard_set_raw_mode(int enabled);

#endif /* KFS_KEYBOARD_H */
