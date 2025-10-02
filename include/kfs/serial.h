#ifndef KFS_SERIAL_H
#define KFS_SERIAL_H

/* serial.h: シリアルポート初期化/出力公開 API (drivers/char/serial.c)。 */

#include <stddef.h>

void serial_init(void);
void serial_write(const char *data, size_t size); /* 既存実装の公開(必要最小限) */

#endif /* KFS_SERIAL_H */
