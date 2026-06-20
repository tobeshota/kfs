#ifndef __KFS_TERMIOS_H
#define __KFS_TERMIOS_H

#include <kfs/stdint.h>

typedef uint32_t tcflag_t;

/* ローカルモードフラグ */
#define ISIG 0000001   /* シグナルの生成を有効にする */
#define ICANON 0000002 /* カノニカルモードを有効にする */
#define ECHO 0000010   /* 入力文字のエコーを有効にする */
#define TOSTOP 0000400 /* バックグラウンド出力時にSIGTTOUを送信する */

/** 制御文字のインデックス
 * @see https://linuxjm.sourceforge.io/html/LDP_man-pages/man3/termios.3.html
 */
#define VINTR 0		/* 割り込み文字 */
#define VQUIT 1		/* 終了文字 */
#define VERASE 2	/* 削除文字 */
#define VKILL 3		/* 行削除文字 */
#define VEOF 4		/* ファイル終了文字 */
#define VTIME 5		/* タイムアウト値 */
#define VMIN 6		/* 最小読み取り文字数 */
#define VSWTC 7		/* スイッチ文字 */
#define VSTART 8	/* 送信開始文字 */
#define VSTOP 9		/* 送信停止文字 */
#define VSUSP 10	/* 一時停止文字 */
#define VEOL 11		/* 行終了文字 */
#define VREPRINT 12 /* 再表示文字 */
#define VDISCARD 13 /* 廃棄文字 */
#define VWERASE 14	/* 単語削除文字 */
#define VLNEXT 15	/* 次の文字をリテラルとして扱う文字 */
#define VEOL2 16	/* 追加の行終了文字 */

#define NCCS 19		   /* 制御文字の数 */
#define KFS_VDISABLE 0 /* 無効な制御文字 */

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
