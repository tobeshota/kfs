#include "../test_reset.h"
#include "unit_test_framework.h"
#include <asm-i386/io.h>
#include <kfs/pcspkr.h>
#include <kfs/timer.h> /* SPEAKER_CTRL_PORT */

/** i8254 PIT を使った PC スピーカードライバが鳴っているかどうかを確認するためのマクロ
 * @brief  port 0x61 の bit0 (Channel 2 Gate) と
 *         bit1 (Speaker enable) を読み取る．
 * @return 0x03 = 鳴っている / 0x00 = 止まっている
 */
#define SPEAKER_IS_ON() (inb(SPEAKER_CTRL_PORT) & 0x03)

static void setup_test(void)
{
	reset_all_state_for_test();
}

static void teardown_test(void)
{
	/* テスト後は必ずスピーカーをオフにする */
	pcspkr_stop();
}

/* pcspkr_init() が port 0x61 の bit0/bit1 を落とすことを確認する */
KFS_TEST(test_pcspkr_init_turns_off_speaker)
{
	pcspkr_init();
	KFS_ASSERT_EQ(0x00, SPEAKER_IS_ON());
}

/** pcspkr_tone() が port 0x61 の bit0/bit1 を立てることを確認する
 * @details bit0 = Channel 2 Gate ON, bit1 = Speaker enable ON
 *          両方が 1 になって初めて音が出る
 */
KFS_TEST(test_pcspkr_tone_enables_speaker)
{
	pcspkr_init();
	pcspkr_tone(440); /* A4 */
	KFS_ASSERT_EQ(0x03, SPEAKER_IS_ON());
}

/* pcspkr_stop() が port 0x61 の bit0/bit1 を落とすことを確認する */
KFS_TEST(test_pcspkr_stop_turns_off_speaker)
{
	pcspkr_tone(440);
	pcspkr_stop();
	KFS_ASSERT_EQ(0x00, SPEAKER_IS_ON());
}

/* pcspkr_tone(0) は pcspkr_stop() と同じくスピーカーをオフにすることを確認する */
KFS_TEST(test_pcspkr_tone_zero_calls_stop)
{
	pcspkr_tone(440);
	pcspkr_tone(0);
	KFS_ASSERT_EQ(0x00, SPEAKER_IS_ON());
}

/* 異なる周波数を連続して設定しても speaker が有効なままであることを確認する */
KFS_TEST(test_pcspkr_tone_retone)
{
	pcspkr_tone(262); /* C4 */
	KFS_ASSERT_EQ(0x03, SPEAKER_IS_ON());
	pcspkr_tone(523); /* C5 */
	KFS_ASSERT_EQ(0x03, SPEAKER_IS_ON());
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_pcspkr_init_turns_off_speaker, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pcspkr_tone_enables_speaker, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pcspkr_stop_turns_off_speaker, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pcspkr_tone_zero_calls_stop, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pcspkr_tone_retone, setup_test, teardown_test),
};

int register_unit_tests_pcspkr(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
