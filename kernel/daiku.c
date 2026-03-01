/**
 * @file kernel/daiku.c
 * @brief よろこびの歌（ベートーベン 第九 第4楽章主題, 1824) — パブリックドメイン
 *
 * ring-3 プロセスとして psg_note() / psg_stop() / msleep() syscall を使い
 * ch0 (メロディ) + ch1 (ベース) を 1 プロセスで同時再生する。
 */

#include <kfs/stdint.h>
#include <kfs/unistd.h> /* psg_note(), psg_stop(), msleep(), exit() */

/* BPM=120 (アレグレット) — 4/4 拍子 */
#define BPM 120
#define Q (60000 / BPM) /* 4 分音符 = 500 ms */
#define QD (Q * 3 / 2)	/* 付点 4 分音符 = 750 ms */
#define H (Q * 2)		/* 2 分音符 = 1000 ms */
#define E (Q / 2)		/* 8 分音符 = 250 ms */
#define FULL (Q * 4)	/* 1 小節分（ベース持続時間） */
#define REST 0
/* 音符ごとに末尾に挿入する無音ギャップ（連続同音を分離する）*/
#define GAP 40 /* ギャップ [ms]: 音符長から差し引く */

/* 音名 → 周波数 (平均律, Hz) — D メジャー */
#define A3 220
#define B3 247
#define CS4 277 /* C#4 */
#define D4 294
#define E4 330
#define FS4 370 /* F#4 */
#define G4 392
#define A4 440

/* ベース用 */
#define D3 147
#define G3 196

struct music_note
{
	uint32_t freq_hz;
	uint32_t ms;
};

/*
 * 1 小節の構造体:
 *   bass_freq — ch1 が FULL 分鳴らすコードルート
 *   melody[]  — ch0 が演奏するノート列（合計 = FULL）、{0,0} で終端
 *   最大 4 音符 + {0,0} ターミネータ → [5]
 */
#define MAX_MELODY_PER_BAR 5
struct bar
{
	uint32_t bass_freq;
	struct music_note melody[MAX_MELODY_PER_BAR];
};

/* 主題 A（8 小節）— E E F#G | G F#E D | C#C#D E | E. D D
 * 正しい旋律: E4 から始まる「みみふぁそ|そふぁみれ|どどれみ|み.れれ」
 */
static const struct bar score[] = {
	/*  1: み み ふぁ そ */ {G3, {{E4, Q}, {E4, Q}, {FS4, Q}, {G4, Q}, {0, 0}}},
	/*  2: そ ふぁ み れ */ {A3, {{G4, Q}, {FS4, Q}, {E4, Q}, {D4, Q}, {0, 0}}},
	/*  3: ど ど れ み   */ {G3, {{CS4, Q}, {CS4, Q}, {D4, Q}, {E4, Q}, {0, 0}}},
	/*  4: み. れ れ     */ {A3, {{E4, QD}, {D4, E}, {D4, H}, {0, 0}, {0, 0}}},
	/*  5: み み ふぁ そ */ {G3, {{E4, Q}, {E4, Q}, {FS4, Q}, {G4, Q}, {0, 0}}},
	/*  6: そ ふぁ み れ */ {A3, {{G4, Q}, {FS4, Q}, {E4, Q}, {D4, Q}, {0, 0}}},
	/*  7: ど ど れ み   */ {G3, {{CS4, Q}, {CS4, Q}, {D4, Q}, {E4, Q}, {0, 0}}},
	/*  8: ど. し し     */ {D3, {{D4, QD}, {CS4, E}, {CS4, H}, {0, 0}, {0, 0}}},
};
#define N_BARS ((int)(sizeof(score) / sizeof(score[0])))

/** よろこびの歌を 1 プロセスで再生する ring-3 メイン関数
 * 各音符末尾に GAP ms の無音を挿入することで連続する同音（DD, BB など）を
 * 明確に分離する。音符長 - GAP だけ発音し、GAP だけ無音にする。
 */
void daiku_main(void *arg)
{
	(void)arg;

	for (int b = 0; b < N_BARS; b++)
	{
		/* ch1: 小節先頭でベースを発音（FULL 分持続） */
		psg_note(1, score[b].bass_freq, (unsigned int)FULL);
		/* ch0: 小節内のメロディを順に鳴らす */
		for (int n = 0; score[b].melody[n].ms != 0; n++)
		{
			const struct music_note *m = &score[b].melody[n];
			uint32_t play_ms = (m->ms > GAP) ? (m->ms - GAP) : m->ms;
			uint32_t gap_ms = (m->ms > GAP) ? GAP : 0;

			if (m->freq_hz != REST)
			{
				psg_note(0, m->freq_hz, m->ms);
			}
			else
			{
				psg_stop(0);
			}
			msleep(play_ms);
			/* 音符間ギャップ: 同音連続をはっきり分離する */
			if (gap_ms > 0)
			{
				psg_stop(0);
				msleep(gap_ms);
			}
		}

		/* 小節末尾: 両チャンネルを消音してから次の小節へ */
		psg_stop(0);
		psg_stop(1);
	}
}
