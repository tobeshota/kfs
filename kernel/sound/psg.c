/** PSG (Programmable Sound Generator) エミュレータ
 * @brief
 * PC スピーカー（i8254 Channel 2）は同時に 1 周波数しか出力できないが，
 * IRQ0 ハンドラから毎 tick 呼ばれる psg_tick() が TDM（時分割多重）により
 * 各チャンネルを順番に出力することで疑似的な多チャンネル発音を実現する．
 *
 * @details PSG とは
 * PSG（Programmable Sound Generator）は 1970〜80 年代のコンピュータ・ゲーム機に
 * 搭載された音源チップの総称（代表例: AY-3-8910, SN76489）．
 * 複数の独立した波形発生器をチップ内に持ち，矩形波・ノイズを同時に出力できる．
 *
 * @details 本物の PSG と kfs の実装の違い
 *   本物の PSG   : チップ内に複数の発振器があり，ハードウェアで同時発音する
 *   kfs の実装   : 発振器は i8254 Channel 2 の 1 つのみ
 *                  → TDM（時分割多重）でソフトウェア的にチャンネルを切り替え，
 *                    人間の聴覚の時間分解能（約 20ms）を利用して疑似的な和音を実現
 *
 * @details
 * TDM スケジュール（HZ=1000 の場合）
 *   tick % 4 == 0  →  ch0 の周波数を PC スピーカーに出力（矩形波）
 *   tick % 4 == 1  →  ch1 の周波数を出力（矩形波）
 *   tick % 4 == 2  →  ch2 の周波数を出力（矩形波）
 *   tick % 4 == 3  →  ch3 の LFSR ノイズ周波数を出力
 * このとき，各チャンネルの実効更新レート = HZ / PSG_CH_COUNT = 250 Hz
 */

#include <kfs/pcspkr.h>
#include <kfs/psg.h>
#include <kfs/timer.h>

static struct psg_channel psg_state[PSG_CH_COUNT];

/** 16bit Galois LFSR の状態
 * @note 初期値は任意の非ゼロ値．
 */
static uint16_t lfsr_state = 0xACE1;

/** LFSR を 1 ステップ進め，可聴ノイズ帯域の周波数を返す
 * @return 1000〜5095 Hz の疑似乱数周波数
 */
static uint32_t noise_lfsr_next(void)
{
	/* 右に1シフト */
	lfsr_state >>= 1;

	/* 右シフト後最下位ビットが1のとき，
	 * Galois LFSR では15,13,12,10ビット目を反転する:
	 * 0xB400 = 1011 0100 0000 0000 = 0xB400 */
	if (lfsr_state & 1)
	{
		lfsr_state ^= 0xB400;
	}

	/* 1kHz〜約5kHz にマッピング
	 * （可聴ノイズとして認識しやすい帯域） */
	return 1000 + (uint32_t)(lfsr_state & 0x0FFF);
}

/* PSG エミュレータを初期化する */
void psg_init(void)
{
	int i;

	for (i = 0; i < PSG_CH_COUNT; i++)
	{
		psg_state[i].freq = 0;
		psg_state[i].active = 0;
		psg_state[i].noise = 0;
	}
	/* 最終チャンネルをノイズ専用に割り当てる */
	psg_state[PSG_CH_COUNT - 1].noise = 1;
}

/* 指定チャンネルで発音を開始する */
void psg_note(int ch, uint32_t freq_hz)
{
	if (ch < 0 || ch >= PSG_CH_COUNT)
	{
		return;
	}

	psg_state[ch].freq = freq_hz;
	if (freq_hz != 0)
	{
		psg_state[ch].active = 1;
	}
	else
	{
		psg_state[ch].active = 0;
	}
}

/* 指定チャンネルを停止する */
void psg_stop(int ch)
{
	psg_note(ch, 0);
}

/** TDM ディスパッチャ
 * @brief 各チャンネルの状態に応じて PC スピーカーに周波数を出力する．
 * @note IRQ0 ハンドラから毎 tick 呼ばれる
 * @note jiffies はこの関数が呼ばれる前に
 *       timer_interrupt() でインクリメントされている．
 */
void psg_tick(void)
{
	int ch;
	int any_active;

	ch = (int)(jiffies % PSG_CH_COUNT);

	/* すべてのチャンネルが停止しているか確認し，
	 * そうである場合はPCスピーカーを停止させる */
	if (!psg_state[ch].active)
	{
		any_active = 0;
		for (int i = 0; i < PSG_CH_COUNT; i++)
		{
			if (psg_state[i].active)
			{
				any_active = 1;
				break;
			}
		}
		if (!any_active)
		{
			pcspkr_stop();
		}
		return;
	}

	/* このチャンネルが発音中である場合，
	 * PC スピーカーに周波数を出力する */
	if (psg_state[ch].noise)
	{
		pcspkr_tone(noise_lfsr_next());
	}
	else
	{
		pcspkr_tone(psg_state[ch].freq);
	}
}
