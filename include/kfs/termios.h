#ifndef __KFS_TERMIOS_H
#define __KFS_TERMIOS_H

#include <kfs/stdint.h>

typedef uint32_t tcflag_t;

/* ローカルモードフラグ */
#define ISIG 0000001   /* シグナルの生成を有効にする */
#define ICANON 0000002 /* カノニカルモードを有効にする */
#define ECHO 0000010   /* 入力文字のエコーを有効にする */

#define NCCS 19 /* 制御文字の数 */

/** TTYの動作設定一式
 * @see https://linuxjm.sourceforge.io/html/LDP_man-pages/man3/termios.3.html
 */
struct termios
{
	tcflag_t c_iflag;		  /* 入力モードフラグ */
	tcflag_t c_oflag;		  /* 出力モードフラグ */
	tcflag_t c_cflag;		  /* 制御モードフラグ */
	tcflag_t c_lflag;		  /* ローカルモードフラグ */
	unsigned char c_line;	  /* 行識別子 */
	unsigned char c_cc[NCCS]; /* 制御文字 */
};

#endif /* __KFS_TERMIOS_H */
