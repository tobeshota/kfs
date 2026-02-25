/** PC スピーカードライバ
 * @brief
 * i8254 PIT Channel 2 を Mode 3 (Square Wave Generator) で動作させ、
 * port 0x61 のゲートビットを操作することで PC スピーカーから矩形波を出力する。
 *
 * @details Channel 2 と PC スピーカーの物理接続
 *
 *   i8254 PIT Channel 2
 *     OUT ピン ─────────────┐
 *                            AND ──→ スピーカーコイル
 *   port 0x61 bit1 (Gate) ──┘
 *
 * Channel 0 (IRQ0 タイマー) とは独立したカウンタであるため、
 * このドライバはタイマー割り込みに一切干渉しない。
 *
 * @details 周波数設定の仕組み
 *   f_out = CLOCK_TICK_RATE / N  [Hz]
 *   N を 0x42 に lobyte → hibyte の順で書き込む。
 *   i8254 は N クロックごとに OUT を反転し続ける（Mode 3 = Square Wave）。
 *
 * @see i8254 データシート MODE 3: SQUARE WAVE GENERATOR
 *      https://www.scs.stanford.edu/10wi-cs140/pintos/specs/8254.pdf
 */

#include <asm-i386/io.h>
#include <kfs/pcspkr.h>
#include <kfs/timer.h> /* CLOCK_TICK_RATE, PIT_MODE_CMD_PORT, PIT_CHANNEL2_PORT,
			  PIT_CH2_SQUARE_WAVE, SPEAKER_CTRL_PORT */

/** PC スピーカードライバを初期化する
 * @brief port 0x61 の bit0 (Gate) と bit1 (Speaker) を落とし、
 *        スピーカーをオフ状態で起動する。
 *        start_kernel() の serial_init() 直後に呼ぶこと。
 */
void pcspkr_init(void)
{
	outb(SPEAKER_CTRL_PORT, inb(SPEAKER_CTRL_PORT) & ~0x03);
}

/** 指定した周波数の矩形波をスピーカーから出力する
 * @param freq_hz 出力周波数 [Hz]。0 を渡すと pcspkr_stop() と同じ動作。
 *
 * @details 動作手順:
 *  1. PIT_CH2_SQUARE_WAVE (0xB6) を PIT_MODE_CMD_PORT (0x43) に書き込み、
 *     Channel 2 を Mode 3 (Square Wave) に設定する
 *  2. カウント値 N = CLOCK_TICK_RATE / freq_hz を PIT_CHANNEL2_PORT (0x42) に
 *     lobyte → hibyte の順で書き込む
 *     → i8254 が 1.193182 MHz クロックで N カウントごとに OUT を反転し続ける
 *  3. port 0x61 の bit0 (Gate ON) と bit1 (Speaker ON) を立てる
 *     → Channel 2 の OUT 矩形波がスピーカーコイルに流れ、音が出る
 *  ※ port 0x61 は RMW（Read-Modify-Write）で他ビットを保持する
 */
void pcspkr_tone(uint32_t freq_hz)
{
	uint16_t count;

	if (freq_hz == 0)
	{
		pcspkr_stop();
		return;
	}

	count = (uint16_t)(CLOCK_TICK_RATE / freq_hz);

	/* Channel 2 を Mode 3 (Square Wave Generator) に設定 */
	outb(PIT_MODE_CMD_PORT, PIT_CH2_SQUARE_WAVE);

	/* カウント値を lobyte → hibyte の順で書き込む */
	outb(PIT_CHANNEL2_PORT, (uint8_t)(count & 0xFF));
	outb(PIT_CHANNEL2_PORT, (uint8_t)((count >> 8) & 0xFF));

	/* Gate ON (bit0) + Speaker ON (bit1)：RMW で他ビットを保持 */
	outb(SPEAKER_CTRL_PORT, inb(SPEAKER_CTRL_PORT) | 0x03);
}

/** PC スピーカーを停止する
 * @brief port 0x61 の bit0 (Gate) と bit1 (Speaker) を落とす。
 *        Channel 2 のカウンタ設定はそのまま保持される。
 */
void pcspkr_stop(void)
{
	outb(SPEAKER_CTRL_PORT, inb(SPEAKER_CTRL_PORT) & ~0x03);
}
