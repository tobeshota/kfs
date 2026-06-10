#ifndef __KFS_IOCTL_H
#define __KFS_IOCTL_H

#include <kfs/termios.h>

/*
 * ioctl();で使用する端末制御用のリクエストコード
 */

#define TCGETS 0x5401	 /* 端末の属性を取得する */
#define TCSETS 0x5402	 /* 端末の属性を設定する */
#define TIOCSCTTY 0x540E /* 指定端末を呼び出し元プロセスの制御端末にする． */

#endif /* __KFS_IOCTL_H */
