#ifndef __KFS_IOCTL_H
#define __KFS_IOCTL_H

/*
 * ioctl();で使用する端末制御用のリクエストコード
 */

/** 指定端末を呼び出し元プロセスの制御端末にする．
 * @note Terminal IO Control Set Controlling TTYの略．
 */
#define TIOCSCTTY 0x540E

#endif /* __KFS_IOCTL_H */
