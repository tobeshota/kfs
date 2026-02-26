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
	uint32_t freq;	 /* 出力周波数 [Hz]．0=停止 */
	uint32_t active; /* 1=発音中, 0=消音 */
	uint32_t noise;	 /* 1=ノイズチャンネル（LFSR 周波数を使う），0=矩形波チャンネル */
};

void psg_init(void);
void psg_note(int ch, uint32_t freq_hz);
void psg_stop(int ch);
void psg_tick(void);

#endif /* _KFS_PSG_H */
