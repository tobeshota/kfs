/**
 * @file kernel/furusato.c
 * @brief ふるさと (高野辰之 作詞 / 岡野貞一 作曲, 1914) — パブリックドメイン
 *
 * ring-3 プロセスとして psg_note() / psg_stop() / msleep() syscall を使い
 * ch0 (メロディ) + ch1 (ベース) を 1 プロセスで同時再生する。
 *
 * 【なぜ fork しないか: PSG 音切れ研究との関係】
 * このカーネルの fork() はページテーブルをコピーするが物理ページは共有のまま
 * (COW 未実装)。2 つの子プロセスが同一の物理スタックを並行書き換えして破壊する。
 * 音切れ (PSG TDM デッドライン・ミス) は ring-0 タイマー割り込みで起きるため
 * ring-3 側のプロセス数とは無関係。1 プロセスから psg_note(0,*) と psg_note(1,*)
 * の両方を呼べば TDM 多重化が始まり、ミス時に「ぴこぴこ」音として観測できる。
 */

#include <kfs/stdint.h>
#include <kfs/unistd.h> /* psg_note(), psg_stop(), msleep(), exit() */

/* BPM=76 (アンダンティーノ) — 3/4 拍子 */
#define BPM 76
#define Q (60000 / BPM) /* 4 分音符 ≈ 789 ms */
#define QD (Q * 3 / 2)	/* 付点 4 分音符 ≈ 1184 ms */
#define E (Q / 2)		/* 8 分音符 ≈ 395 ms */
#define HD (Q * 3)		/* 付点 2 分音符（1 小節分）≈ 2368 ms */
#define REST 0

/* 音名 → 周波数 (平均律, Hz) — C メジャー */
#define G3 196
#define C4 262
#define D4 294
#define E4 330
#define F4 349
#define G4 392
#define A4 440
#define C5 523

struct music_note
{
	uint32_t freq_hz;
	uint32_t ms;
};

/*
 * 1 小節の構造体:
 *   bass_freq — ch1 が HD 分鳴らすコードルート
 *   melody[]  — ch0 が演奏するノート列（合計 = HD）、{0,0} で終端
 */
#define MAX_MELODY_PER_BAR 4
struct bar
{
	uint32_t bass_freq;
	struct music_note melody[MAX_MELODY_PER_BAR];
};

static const struct bar score[] = {
	/*  1: うさぎ         */ {G3, {{G4, Q}, {G4, Q}, {G4, Q}, {0, 0}}},
	/*  2: おいしかのやま */ {C4, {{E4, QD}, {C4, E}, {D4, Q}, {0, 0}}},
	/*  3: かのやま       */ {C4, {{E4, Q}, {E4, Q}, {E4, Q}, {0, 0}}},
	/*  4: やまより       */ {G3, {{E4, QD}, {D4, E}, {G4, Q}, {0, 0}}},
	/*  5: こぶな         */ {G3, {{G4, Q}, {G4, Q}, {G4, Q}, {0, 0}}},
	/*  6: つりしかのかわ */ {A4, {{A4, QD}, {G4, E}, {E4, Q}, {0, 0}}},
	/*  7: かのかわ       */ {G3, {{D4, Q}, {E4, Q}, {G4, Q}, {0, 0}}},
	/*  8: かわ           */ {C4, {{C4, HD}, {0, 0}, {0, 0}, {0, 0}}},
	/*  9: ゆめは         */ {C4, {{E4, Q}, {E4, Q}, {E4, Q}, {0, 0}}},
	/* 10: いまもめぐりて */ {G3, {{G4, QD}, {E4, E}, {C4, Q}, {0, 0}}},
	/* 11: めぐりて       */ {G3, {{D4, Q}, {D4, Q}, {D4, Q}, {0, 0}}},
	/* 12: て             */ {G3, {{D4, HD}, {0, 0}, {0, 0}, {0, 0}}},
	/* 13: わすれが       */ {C4, {{E4, Q}, {F4, Q}, {G4, Q}, {0, 0}}},
	/* 14: たきふるさと   */ {A4, {{A4, QD}, {G4, E}, {E4, Q}, {0, 0}}},
	/* 15: ふるさと       */ {G3, {{G4, Q}, {E4, Q}, {C4, Q}, {0, 0}}},
	/* 16: さと           */ {C4, {{C4, HD}, {0, 0}, {0, 0}, {0, 0}}},
};
#define N_BARS ((int)(sizeof(score) / sizeof(score[0])))

/**
 * ふるさとを 1 プロセスで再生する ring-3 メイン関数。
 *
 * 各小節の先頭で ch1 にベース音を発音し ch0 でメロディを演奏する。
 * ch1 持続時間 HD = ch0 の 1 小節合計時間が等しいため
 * msleep() を ch0 ノートごとに呼ぶだけで 2 チャンネルが自然に重なる。
 *
 * PSG エミュレータは ch0/ch1 を 1ms 周期で TDM 切り替えするため
 * msleep() スリープ中も切り替えは続く。スケジューラ遅延や割り込みにより
 * タイマー期限に間に合わなかった場合は「ぴこぴこ」音として観測される。
 */
void furusato_main(void)
{
	for (int b = 0; b < N_BARS; b++)
	{
		/* ch1: 小節先頭でベースを発音（HD 分持続） */
		psg_note(1, score[b].bass_freq);

		/* ch0: 小節内のメロディを順に鳴らす */
		for (int n = 0; score[b].melody[n].ms != 0; n++)
		{
			const struct music_note *m = &score[b].melody[n];
			if (m->freq_hz != REST)
			{
				psg_note(0, m->freq_hz);
			}
			else
			{
				psg_stop(0);
			}
			msleep(m->ms);
		}

		/* 小節末尾: 両チャンネルを消音してから次の小節へ */
		psg_stop(0);
		psg_stop(1);
	}
}
