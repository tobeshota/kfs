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

#include <kfs/exit.h>
#include <kfs/pcspkr.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/sched.h>
#include <kfs/timer.h>

static volatile struct psg_channel psg_state[PSG_CH_COUNT];

/** 最後に do_psg_note() を呼んだプロセスの PID（sched_ext の audio_ops が参照） */
static volatile uint32_t psg_caller_pid = 0;

/** TDM ローター: 次に試すチャンネル番号 */
static volatile int psg_rotor = 0;

/** 現在 PC スピーカーで発音中のチャンネル番号 */
static volatile int psg_current_ch = 0;

/** 現チャンネルの残り発音 tick 数
 * @note 0 になったら次のアクティブチャンネルへ切り替える
 */
static volatile int psg_slot_remaining = 0;

/** exit hook の登録済みフラグ */
static int psg_exit_hook_registered = 0;

/** 各チャンネルの連続発音時間 [tick = ms]
 * @note 最低音 G3=196Hz の周期は 5.1ms なので 4 周期 = 20ms 必要。
 *       2ch 使用時の切り替え周期 = 40ms (25Hz) で聴覚統合閾値(約 20Hz)を超える。
 */
#define PSG_SLOT_TICKS 20

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

	if (!psg_exit_hook_registered)
	{
		psg_init_exit_hook();
		psg_exit_hook_registered = 1;
	}

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
void do_psg_note(int ch, uint32_t freq_hz, uint32_t deadline_ms)
{
	if (ch < 0 || ch >= PSG_CH_COUNT)
	{
		return;
	}

	/* 呼び出し元 PID を記録（sched_ext audio_ops が参照） */
	psg_caller_pid = (uint32_t)current->pid;

	/* 音切れ検出: 前回設定した deadline を過ぎていたら glitch とみなす */
	if (psg_state[ch].deadline_tick != 0 && jiffies > psg_state[ch].deadline_tick)
	{
		psg_state[ch].glitch_count++;
		psg_state[ch].glitch_delay += jiffies - psg_state[ch].deadline_tick;
	}

	psg_state[ch].freq = freq_hz;
	if (freq_hz != 0)
	{
		psg_state[ch].active = 1;
		/* 新しい deadline_tick を記録 */
		psg_state[ch].deadline_tick = (deadline_ms != 0) ? (jiffies + deadline_ms) : 0;
		/* 現在このチャンネルがスロット中なら周波数を即時反映 */
		if (ch == psg_current_ch && psg_slot_remaining > 0)
		{
			pcspkr_tone(freq_hz);
		}
	}
	else
	{
		psg_state[ch].active = 0;
		psg_state[ch].deadline_tick = 0;
		if (ch == psg_current_ch)
		{
			psg_slot_remaining = 0;
		}
	}
}

/* 指定チャンネルを停止する */
void do_psg_stop(int ch)
{
	do_psg_note(ch, 0, 0);
}

/** TDM ディスパッチャ
 * @brief 各チャンネルの状態に応じて PC スピーカーに周波数を出力する．
 * @note IRQ0 ハンドラから毎 tick (1ms) 呼ばれる
 * @details 各チャンネルを PSG_SLOT_TICKS ms 連続発音してから次へ切り替える。
 *
 *  旧実装（1ms スロット, ch0+ch1 有効）:
 *    [ch0=1ms][ch1=1ms][ch0=1ms]... → 切り替え周期 2ms = 500Hz → ぴこぴこ
 *
 *  新実装（20ms スロット, ch0+ch1 有効）:
 *    [ch0=20ms][ch1=20ms][ch0=20ms]... → 切り替え周期 40ms = 25Hz
 *    G3(196Hz) は 20ms で約 4 周期 → 音程として認識可能
 */
void psg_tick(void)
{
	int tried;

	/* 現チャンネルの残り時間があれば i8254 は継続発音中 → 何もしない */
	if (psg_slot_remaining > 0)
	{
		psg_slot_remaining--;
		return;
	}

	/* スロット切れ: 次のアクティブチャンネルを探す */
	tried = 0;
	while (tried < PSG_CH_COUNT)
	{
		int ch = psg_rotor;
		psg_rotor = (psg_rotor + 1) % PSG_CH_COUNT;
		tried++;

		if (!psg_state[ch].active)
		{
			continue;
		}

		if (psg_state[ch].noise)
		{
			pcspkr_tone(noise_lfsr_next());
		}
		else
		{
			pcspkr_tone(psg_state[ch].freq);
		}
		psg_current_ch = ch;
		psg_slot_remaining = PSG_SLOT_TICKS - 1;
		return;
	}

	/* アクティブなチャンネルが 1 つもない → スピーカー停止 */
	pcspkr_stop();
}

/* 各チャンネルの音切れ統計を表示する */
void psg_glitch_stat(void)
{
	static const char *ch_name[PSG_CH_COUNT] = {"melody", "bass", "harmony", "noise"};
	printk("PSG glitch statistics:\n");
	for (int i = 0; i < PSG_CH_COUNT; i++)
	{
		printk("  ch%d (%s): %u glitches, %u ms total delay\n", i, ch_name[i], psg_state[i].glitch_count,
			   psg_state[i].glitch_delay);
	}
}

/* 全チャンネルの音切れカウンタをリセットする */
void psg_glitch_reset(void)
{
	for (int i = 0; i < PSG_CH_COUNT; i++)
	{
		psg_state[i].glitch_count = 0;
		psg_state[i].glitch_delay = 0;
		psg_state[i].deadline_tick = 0;
	}
	printk("PSG glitch counters reset.\n");
}

/* 最後に do_psg_note() を呼んだプロセスの PID を返す（audio_ops 用） */
uint32_t psg_get_caller_pid(void)
{
	return psg_caller_pid;
}

/* 指定 PID のプロセスが PSG を使っていたなら全チャンネルを停止する */
static void psg_exit_hook(struct task_struct *tsk)
{
	if (!tsk || psg_caller_pid != (uint32_t)tsk->pid)
	{
		return;
	}

	for (int ch = 0; ch < PSG_CH_COUNT; ch++)
	{
		do_psg_stop(ch);
	}
	psg_caller_pid = 0;
}

/* PSG の終了フックを登録する */
void psg_init_exit_hook(void)
{
	register_exit_hook(psg_exit_hook);
}
