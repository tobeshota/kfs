#ifndef _KFS_TIMER_H
#define _KFS_TIMER_H

#include <kfs/stdint.h>

/* タイマー周波数 */
#define HZ 1000					/* ヘルツ（1秒あたりのタイマー割り込み回数） */
#define CLOCK_TICK_RATE 1193182 /* i8254 PIT 入力クロック周波数 [Hz] */

/** i8254 PIT I/O port addresses
 * @brief チャネル 0〜2 はそれぞれ独立したカウンタを持つ
 */
#define PIT_MODE_CMD_PORT 0x43 /* モードコマンドレジスタ（書き込み専用） */
#define PIT_CHANNEL0_PORT 0x40 /* チャネル 0 データポート */

/** i8254 PITのチャネル0をモード2（レートジェネレータ）に設定するコマンドワード
 * @brief タイマー割り込みを一定の周期で発生させるためのコマンドワード
 * @details PIT_CH0_RATE_GEN = 0x34 = 0011 0100
 *  bit 7-6 = 00  : チャネル 0 選択
 *  bit 5-4 = 11  : lobyte/hibyte モード
 *                  （下位バイト → 上位バイトの順で書き込み）
 *  bit 3-1 = 010 : モード 2（レートジェネレータ）
 *                  1.193182 MHz クロックで毎サイクルcountをデクリメントし，
 *                  countが1のときOUTピンを1クロック幅LOWパルスにして
 *                  8259A PICにIRQ0の発生を要求し，
 *                  次のクロックでcountを初期値(1193)に戻し，
 *                  以降も同様にcountをデクリメントし続ける（自動リロード）．
 *  bit 0   = 0   : バイナリカウント
 *
 * @see MODE 2: RATE GENERATOR
 *      at https://www.scs.stanford.edu/10wi-cs140/pintos/specs/8254.pdf
 */
#define PIT_CH0_RATE_GEN 0x34

void timer_init(void);

#endif /* _KFS_TIMER_H */
