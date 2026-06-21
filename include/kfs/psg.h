#ifndef _KFS_PSG_H
#define _KFS_PSG_H

#include <kfs/stdint.h>

/** チャンネル数
 * @brief
 * ch0-ch2 : 矩形波チャンネル
 * ch3     : LFSR ノイズチャンネル
 */
#define PSG_CH_COUNT 4

/* PSG チャンネル状態 */
struct psg_channel
{
	uint32_t freq;			/* 出力周波数 [Hz]．0=停止 */
	uint32_t active;		/* 1=発音中, 0=消音 */
	uint32_t noise;			/* 1=ノイズチャンネル（LFSR 周波数を使う），0=矩形波チャンネル */
	uint32_t deadline_tick; /* 次の psg_note() が来るべき jiffies 値 (0=無効) */
	uint32_t glitch_count;	/* deadline 超過回数 */
	uint32_t glitch_delay;	/* 累積超過 tick 数（≒ ms） */
};

void psg_init(void);
void do_psg_note(int ch, uint32_t freq_hz, uint32_t deadline_ms);
void do_psg_stop(int ch);
void psg_tick(void);
void psg_glitch_stat(void);
void psg_glitch_reset(void);
uint32_t psg_get_caller_pid(void);
void psg_init_process_hooks(void);

#endif /* _KFS_PSG_H */
