#include <kfs/piano.h>
#include <kfs/psg.h>
#include <kfs/shell.h>
#include <kfs/stdio.h>
#include <kfs/unistd.h>

#define PIANO_MELODY_CH_MAX 3

#define NOTE_B3 247U
#define NOTE_C4 262U
#define NOTE_CS4 277U
#define NOTE_D4 294U
#define NOTE_DS4 311U
#define NOTE_E4 330U
#define NOTE_F4 349U
#define NOTE_FS4 370U
#define NOTE_G4 392U
#define NOTE_GS4 415U
#define NOTE_A4 440U
#define NOTE_AS4 466U
#define NOTE_B4 494U
#define NOTE_C5 523U
#define NOTE_CS5 554U
#define NOTE_DS5 622U
#define NOTE_D5 587U
#define NOTE_E5 659U

static int g_piano_active;
static int g_shift_pressed;
static int g_space_pressed;
static int g_key_channel[KEYBOARD_SCANCODE_MAX];

static void piano_reset_state(void)
{
	int i;

	g_shift_pressed = 0;
	g_space_pressed = 0;
	for (i = 0; i < KEYBOARD_SCANCODE_MAX; i++)
	{
		g_key_channel[i] = -1;
	}
}

static void piano_stop_all(void)
{
	int ch;

	for (ch = 0; ch < 4; ch++)
	{
		psg_stop(ch);
	}
	piano_reset_state();
}

static uint32_t piano_key_freq(uint8_t code)
{
	switch (code)
	{
	case 0x10:
		return NOTE_B3;
	case 0x11:
		return NOTE_CS4;
	case 0x12:
		return NOTE_DS4;
	case 0x14:
		return NOTE_FS4;
	case 0x15:
		return NOTE_GS4;
	case 0x16:
		return NOTE_AS4;
	case 0x18:
		return NOTE_CS5;
	case 0x19:
		return NOTE_DS5;
	case 0x1E:
		return NOTE_C4;
	case 0x1F:
		return NOTE_D4;
	case 0x20:
		return NOTE_E4;
	case 0x21:
		return NOTE_F4;
	case 0x22:
		return NOTE_G4;
	case 0x23:
		return NOTE_A4;
	case 0x24:
		return NOTE_B4;
	case 0x25:
		return NOTE_C5;
	case 0x26:
		return NOTE_D5;
	case 0x27:
		return NOTE_E5;
	default:
		return 0;
	}
}

static int piano_alloc_channel(void)
{
	int ch;

	for (ch = 0; ch < PIANO_MELODY_CH_MAX; ch++)
	{
		int used = 0;
		int i;

		for (i = 0; i < KEYBOARD_SCANCODE_MAX; i++)
		{
			if (g_key_channel[i] == ch)
			{
				used = 1;
				break;
			}
		}
		if (!used)
		{
			return ch;
		}
	}
	return -1;
}

static uint32_t piano_apply_modifiers(uint32_t freq)
{
	if (g_shift_pressed && !g_space_pressed)
	{
		return freq / 2U;
	}
	if (g_space_pressed && !g_shift_pressed)
	{
		return freq * 2U;
	}
	return freq;
}

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
void cmd_piano(void *arg)
{
	(void)arg;
	setpgid(0, 0);
	g_piano_active = 1;
	piano_reset_state();
	kbd_clear_events();
	kbd_set_raw_mode(1);
	printf("piano: Esc=exit, Shift=octave down, Space=octave up, Enter=noise\n");
	while (g_piano_active)
	{
		struct kfs_keyboard_raw_event event;
		if (kbd_read_event(&event) < 0)
		{
			break;
		}
		piano_raw_handler(event.code, event.release);
	}
	kbd_set_raw_mode(0);
	piano_stop_all();
}

int piano_raw_handler(uint8_t code, int release)
{
	uint32_t freq;
	int ch;

	if (code >= KEYBOARD_SCANCODE_MAX)
	{
		return 1;
	}

	if (code == 0x01 && !release)
	{
		g_piano_active = 0;
		piano_stop_all();
		return 1;
	}

	if (code == 0x2A || code == 0x36)
	{
		g_shift_pressed = release ? 0 : 1;
		return 1;
	}

	if (code == 0x39)
	{
		g_space_pressed = release ? 0 : 1;
		return 1;
	}

	if (code == 0x1C)
	{
		if (release)
		{
			psg_stop(3);
		}
		else
		{
			psg_note(3, 1, 0);
		}
		return 1;
	}

	freq = piano_key_freq(code);
	if (freq == 0)
	{
		return 1;
	}

	if (release)
	{
		if (g_key_channel[code] >= 0)
		{
			psg_stop(g_key_channel[code]);
			g_key_channel[code] = -1;
		}
		return 1;
	}

	if (g_key_channel[code] >= 0)
	{
		return 1;
	}

	ch = piano_alloc_channel();
	if (ch < 0)
	{
		return 1;
	}

	g_key_channel[code] = ch;
	psg_note(ch, piano_apply_modifiers(freq), 0);
	return 1;
}

int piano_is_active(void)
{
	return g_piano_active;
}
