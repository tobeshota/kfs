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

/* CLOCK_TICK_RATE, PIT_MODE_CMD_PORT,
   PIT_CHANNEL2_PORT, PIT_CH2_SQUARE_WAVE, SPEAKER_CTRL_PORT */
#include <kfs/timer.h>

/** PC スピーカードライバを初期化する
 * @brief スピーカーをオフ状態で起動する．
 * @note start_kernel() の serial_init() 直後に呼ぶこと．
 */
void pcspkr_init(void)
{
	pcspkr_stop();
}

/** 指定した周波数の矩形波をスピーカーから出力する
 * @param freq_hz 出力周波数 [Hz]．
 *        0 を渡すと pcspkr_stop() と同じ動作。
 * @warning PSG が有効な場合（psg_init() 呼び出し後）は直接呼んでも音は鳴らない．
 *          psg_tick() が毎 IRQ0 tick に全 PSG チャンネルを確認し，
 *          全チャンネルが inactive であれば即座に pcspkr_stop() を呼ぶため，
 *          この関数で鳴らした音が次の tick で止められてしまう．
 *          代わりに psg_note() で PSG チャンネルを active にして音を鳴らすこと．
 */
void pcspkr_tone(uint32_t freq_hz)
{
	uint16_t n;

	if (freq_hz == 0)
	{
		pcspkr_stop();
		return;
	}

	/* PIT_CH2_SQUARE_WAVE (0xB6) を PIT_MODE_CMD_PORT (0x43) に書き込み，
	 * Channel 2 を Mode 3 (Square Wave Generator) に設定する */
	outb(PIT_MODE_CMD_PORT, PIT_CH2_SQUARE_WAVE);

	/* カウント値 n を PIT_CHANNEL2_PORT (0x42) に lobyte → hibyte の順で書き込む．
	 * これにより，i8254 が n クロックごとに OUT を反転し続ける（Mode 3 = Square Wave） */
	n = (uint16_t)(CLOCK_TICK_RATE / freq_hz);
	outb(PIT_CHANNEL2_PORT, (uint8_t)(n & 0xFF));
	outb(PIT_CHANNEL2_PORT, (uint8_t)((n >> 8) & 0xFF));

	/** SPEAKER_CTRL_PORT (0x61) の bit0 (Gate) と bit1 (Speaker) を立てる．
	 *  これにより Channel 2 の OUT 矩形波がスピーカーコイルに流れ，音が出る．
	 * @note
	 * inb() で一度読んでから bit0,1 だけ変えて書き戻す（Read-Modify-Write）．
	 * SPEAKER_CTRL_PORT (0x61) の他のビットには NMI 制御など無関係な機能があるため，
	 * 直接 outb(0x03) と書くと他ビットが 0 に上書きされてしまうため
	 */
	outb(SPEAKER_CTRL_PORT, inb(SPEAKER_CTRL_PORT) | 0x03);
}

/** PC スピーカーを停止する
 * @brief port 0x61 の bit0 (Gate) と bit1 (Speaker) を落とす。
 *        Channel 2 のカウンタ設定はそのまま保持される。
 * @warning PSG が有効な場合，この関数を直接呼ぶと発音中チャンネルの音も
 *          強制停止される．チャンネル単位で止めたい場合は psg_stop() を使うこと．
 *          この関数は psg_tick() から全チャンネル inactive 時のみ呼ばれる．
 */
void pcspkr_stop(void)
{
	/* 同様に Read-Modify-Write で bit0,1 だけを 0 にして音を止める */
	outb(SPEAKER_CTRL_PORT, inb(SPEAKER_CTRL_PORT) & ~0x03);
}
