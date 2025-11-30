#ifndef KFS_SERIAL_H
#define KFS_SERIAL_H

/* serial.h: シリアルポート初期化/出力公開 API (drivers/char/serial.c)。 */

#include <kfs/stddef.h>

void serial_init(void);
void serial_write(const char *data, size_t size); /* 既存実装の公開(必要最小限) */
int serial_read(void);							  /* シリアル入力: データがない場合は-1を返す */

#endif /* KFS_SERIAL_H */
