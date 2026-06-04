#ifndef KFS_PIANO_H
#define KFS_PIANO_H

#include <kfs/stdint.h>

/* キーボードコールバック: スキャンコード -> PSG 制御。 */
int piano_raw_handler(uint8_t code, int release);

/* piano モードがアクティブなら 1、そうでなければ 0 を返す。 */
int piano_is_active(void);

void cmd_piano(const char *args, int foreground);

#endif /* KFS_PIANO_H */
