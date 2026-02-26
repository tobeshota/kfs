#ifndef _KFS_TIMER_H
#define _KFS_TIMER_H

#include <kfs/stdint.h>

/* タイマー周波数 */
#define HZ 1000					/* ヘルツ（1秒あたりのタイマー割り込み回数） */
#define CLOCK_TICK_RATE 1193182 /* i8254 PIT 入力クロック周波数 [Hz] */

extern volatile uint32_t jiffies;

/** tick 比較マクロ
 * @param a  「後（larger）」であると期待する jiffies 値
 * @param b  「前（smaller）」であると期待する jiffies 値
 * @details
 *  jiffies は uint32_t なので 0xFFFFFFFF の次は 0x00000000 に折り返す。
 *  単純な `a > b` 比較はこの折り返しで誤判定を起こす:
 *
 *    例) jiffies=0xFFFFFF00, expires=0xFFFFFF00+100=0x00000063
 *    0x00000063 > 0xFFFFFF00  →  false  ← 間違い（折り返しで大小が逆転）
 *
 *  `time_after(a, b)` は差分を int32_t にキャストして符号で判定する:
 *    (int32_t)((b) - (a)) < 0
 *
 *    b=0xFFFFFF00, a=0x00000063 のとき:
 *      0xFFFFFF00 - 0x00000063 = 0xFFFFFF9D
 *      int32_t として解釈 → -99（負）→ true：a の方が後
 *
 * @note
 * この手法は差分が INT32_MAX (約 24.8 日分) を超えないことを前提とする。
 * タイマー満了まで 24.8 日を超えるような値は設定しないこと。
 */
#define time_after(a, b)  ((int32_t)((b) - (a)) < 0)
#define time_before(a, b) time_after(b, a)

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

/** i8254 PIT Channel 2 (PC スピーカー用)
 * @brief Channel 0 (IRQ0 タイマー) とは独立したカウンタ。干渉しない。
 */
#define PIT_CHANNEL2_PORT 0x42 /* チャネル 2 データポート */

/** i8254 PITのチャネル2をモード3（矩形波ジェネレータ）に設定するコマンドワード
 * @details PIT_CH2_SQUARE_WAVE = 0xB6 = 1011 0110
 *  bit 7-6 = 10  : チャネル 2 選択
 *  bit 5-4 = 11  : lobyte/hibyte モード（下位バイト → 上位バイトの順で書き込み）
 *  bit 3-1 = 011 : モード 3（矩形波ジェネレータ）
 *                  N/2 クロック HIGH → N/2 クロック LOW を繰り返す完全な矩形波。
 *                  スピーカーコイルを駆動するには Mode 2 のような
 *                  「ほぼ HIGH、一瞬だけ LOW」のパルスではなく
 *                  均等な矩形波が必要。
 *  bit 0   = 0   : バイナリカウント
 *
 * @see MODE 3: SQUARE WAVE MODE
 *      at https://www.scs.stanford.edu/10wi-cs140/pintos/specs/8254.pdf
 */
#define PIT_CH2_SQUARE_WAVE 0xB6

/** PC スピーカー制御ポート (System Control Port B)
 * @details
 *  bit 0 (r/w): Channel 2 Gate — 1 でカウント開始、0 で停止
 *  bit 1 (r/w): Speaker enable — 1 で Channel 2 OUT をスピーカーコイルに接続
 *  bit 2〜7   : 他用途（NMI 制御等）。触らない。
 *  ※ bit 0,1 を共に 1 にすることで初めて音が出る。RMW が必要。
 */
#define SPEAKER_CTRL_PORT 0x61

void timer_init(void);

#endif /* _KFS_TIMER_H */
