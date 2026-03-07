#include <kfs/keyboard.h>
#include <kfs/piano.h>
#include <kfs/printk.h>
#include <kfs/psg.h>
#include <kfs/sched.h>
#include <kfs/stdint.h>

/** ピアノの鍵盤配列
 * @brief キーボードのスキャンコードをピアノの音程にマッピングするための配列．
 * @details 標準ピアノ鍵盤配列:
 *   上段: q  w  e  .  t  y  u  .  o  p
 *         B3 C#4 D#4   F#4 G#4 A#4   C#5 D#5
 *   下段:  a  s  d  f  g  h  j  k  l  ;
 *         C4 D4  E4  F4  G4  A4 B4  C5  D5  E5
 *                     (. = 黒鍵なし)
 * @note 0は「音程未割当」を意味する．
 *       例えば r (0x13) と i (0x17) は黒鍵がないため未割当である．
 */
static const uint32_t piano_freq[KEYBOARD_SCANCODE_MAX] = {
	/* 下段: 白鍵 C4-E5 */
	[0x1E] = 262, /* a = C4  */
	[0x1F] = 294, /* s = D4  */
	[0x20] = 330, /* d = E4  */
	[0x21] = 349, /* f = F4  */
	[0x22] = 392, /* g = G4  */
	[0x23] = 440, /* h = A4  */
	[0x24] = 494, /* j = B4  */
	[0x25] = 523, /* k = C5  */
	[0x26] = 587, /* l = D5  */
	[0x27] = 659, /* ; = E5  */
	/* 上段: 黒鍵 + q=B3（C4 の左隣白鍵） */
	[0x10] = 247, /* q = B3  (C4 左隣) */
	[0x11] = 277, /* w = C#4 (a-s間) */
	[0x12] = 311, /* e = D#4 (s-d間) */
	/* r (0x13): E4-F4 間に黒鍵なし → 未割当 */
	[0x14] = 370, /* t = F#4 (f-g間) */
	[0x15] = 415, /* y = G#4 (g-h間) */
	[0x16] = 466, /* u = A#4 (h-j間) */
	/* i (0x17): B4-C5 間に黒鍵なし → 未割当 */
	[0x18] = 554, /* o = C#5 (k-l間) */
	[0x19] = 622, /* p = D#5 (l-;間) */
};

/*
 * そのキーが現在どの PSG チャンネル(0-2)を使っているかを記録する。
 * -1 は「未押下」を意味する。キー解放時に正しいチャンネルを止めるために必要。
 */
static int8_t key_ch[KEYBOARD_SCANCODE_MAX];

/*
 * 次に割り当てる PSG チャンネル番号(0-2 を巡回)。
 * 和音を支援するため、新しい音は毎回異なるチャンネルへ振り分ける。
 */
static int next_ch;

/*
 * 0 になると cmd_piano() の schedule() ループが終了し演奏モードを抜ける。
 * IRQ ハンドラから書くため volatile で宣言する。
 */
static volatile int piano_active;

/* shift 押下中フラグ: 1 のとき全音程を 1 オクターブ下げる(÷2) */
static int piano_shift;

/* space 押下中フラグ: 1 のとき全音程を 1 オクターブ上げる(×2) */
static int piano_octave_up;

/*
 * shift や space の状態が変わったとき、
 * すでに押下中のキーに対して新しい周波数で即座に再発音する。
 * これにより「オクターブ変更がリアルタイムに反映」される。
 */
static void retrigger_held_keys(void)
{
	int i;
	for (i = 0; i < KEYBOARD_SCANCODE_MAX; i++)
	{
		/* key_ch[i] < 0 は未押下なのでスキップ */
		if (key_ch[i] < 0)
		{
			continue;
		}
		uint32_t freq = piano_freq[i];
		/* piano_freq が 0 のキーは音程未割当なのでスキップ */
		if (freq == 0)
		{
			continue;
		}
		/* 現在のオクターブ修飾を適用して再発音 */
		if (piano_shift)
		{
			/* 1オクターブ下げ: 周波数を半分に */
			freq /= 2;
		}
		if (piano_octave_up)
		{
			/* 1オクターブ上げ: 周波数を2倍に */
			freq *= 2;
		}
		do_psg_note(key_ch[i], freq, 0);
	}
}

/*
 * キーボード IRQ コンテキストから EOI 送信前に呼ばれる。
 * PSG レジスタを直接操作するだけで済むため、ブロックや sleep は不要。
 * 戻り値 1 は「このスキャンコードを通常の ASCII 変換処理に渡さない」を意味する。
 */
static int piano_raw_handler(uint8_t code, int release)
{
	uint32_t freq;
	int ch;

	/* Escape 押下
	 * piano_active を 0 にして piano_loop() を終了させる */
	if (code == 0x01 && !release)
	{
		piano_active = 0;
		return 1;
	}

	/* shift キー(左:0x2A 右:0x36)
	 * オクターブ下げフラグを更新し、押下中キーをすぐに新しい周波数で再発音する */
	if (code == 0x2A || code == 0x36)
	{
		piano_shift = release ? 0 : 1;
		retrigger_held_keys();
		return 1;
	}

	/* Enter キー
	 * PSG ch3 の LFSR ノイズを press/release で on/off する。
	 * ch0-2 の矩形波とは独立したチャンネルを使うため和音と共存できる。 */
	if (code == 0x1C)
	{
		if (!release)
		{
			do_psg_note(3, 3000, 0);
		}
		else
		{
			do_psg_stop(3);
		}
		return 1;
	}

	/* スペースキー
	 * オクターブ上げフラグを更新し、押下中キーを即再発音する。
	 * トグルではなく「押している間だけ上げる」方式を採用している。 */
	if (code == 0x39)
	{
		piano_octave_up = release ? 0 : 1;
		retrigger_held_keys();
		return 1;
	}

	/* スキャンコードに対応する基本周波数を引く。未割当(0)なら何もしない */
	freq = (code < KEYBOARD_SCANCODE_MAX) ? piano_freq[code] : 0;
	if (freq == 0)
	{
		return 1;
	}

	/* shift/space によるオクターブ修飾を新規押下のキーに適用する。
	 * retrigger_held_keys() は shift/space の「状態変化時」に
	 * すでに押下中のキーを再発音するためのものであり、
	 * 「shift/space を押したまま新たに押したキー」には呼ばれない。
	 * そのため、ここで改めて修飾を適用する必要がある。 */
	if (piano_shift)
	{
		freq /= 2; /* 1オクターブ下げ: 周波数を半分に */
	}
	if (piano_octave_up)
	{
		freq *= 2; /* 1オクターブ上げ: 周波数を2倍に */
	}

	if (!release)
	{
		/*
		 * キー押下: すでに同じキーが鳴っていれば何もしない(連打防止)。
		 * 新規押下なら次の空きチャンネルに割り当てて発音開始する。
		 */
		if (key_ch[code] >= 0)
		{
			return 1;
		}
		ch = next_ch;
		next_ch = (next_ch + 1) % 3; /* チャンネルを巡回して和音を可能にする */
		key_ch[code] = (int8_t)ch;
		do_psg_note(ch, freq, 0);
	}
	else
	{
		/*
		 * キー解放: key_ch から割り当てチャンネルを取り出して発音を止める。
		 * key_ch[code] を -1 に戻しておくことで再押下を受け付ける状態にする。
		 */
		ch = key_ch[code];
		if (ch >= 0)
		{
			key_ch[code] = -1;
			do_psg_stop(ch);
		}
	}
	return 1;
}

void cmd_piano(void)
{
	int i;

	/* 操作方法をカーネルログに表示する */
	printk("piano: [white] a-; = C4-E5  q=B3\n");
	printk("piano: [black] w=C# e=D#  t=F# y=G# u=A#  o=C# p=D#\n");
	printk("piano: shift+key = 1 octave down   space+key = 1 octave up   Enter = noise   Esc = quit\n");

	/* 状態を初期化する: 全キーを未押下(-1)にしてチャンネルカウンタをリセット */
	for (i = 0; i < KEYBOARD_SCANCODE_MAX; i++)
	{
		key_ch[i] = -1;
	}
	next_ch = 0;
	piano_shift = 0;
	piano_octave_up = 0;
	piano_active = 1;

	/* raw_handler を登録して以降のキーボード IRQ を piano に横取りさせる */
	kfs_keyboard_set_raw_handler(piano_raw_handler);

	/* piano_active が 0 になるまで schedule() を繰り返す。
	 * schedule() で CPU を手放すたびにキーボード IRQ が処理され、
	 * piano_raw_handler が PSG 状態を更新できるようになる。
	 * hlt ループではなく schedule() を使うことでタイマー割り込みや
	 * 他プロセスも並行して動作し続ける。 */
	while (piano_active)
	{
		schedule();
	}

	/* 演奏終了後: raw_handler を外して通常キーボード処理に戻す */
	kfs_keyboard_set_raw_handler(NULL);

	/* 押しっぱなしになったまま終了した場合に備えて全 PSG チャンネルを止める */
	for (i = 0; i < 4; i++)
	{
		do_psg_stop(i);
	}

	printk("piano: bye.\n");
}
