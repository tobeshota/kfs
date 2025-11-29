#include "../../../test_reset.h"
#include "unit_test_framework.h"
#include <asm-i386/i8259.h>
#include <kfs/stdint.h>

/* セットアップ */
static void setup_test(void)
{
	reset_all_state_for_test();
}

static void teardown_test(void)
{
}

/* ========== i8259.hの定数テスト ========== */

/** PICポートアドレス定数検証
 * 検証対象: PIC_MASTER_CMD, PIC_MASTER_IMR, PIC_SLAVE_CMD, PIC_SLAVE_IMR
 * 検証項目: PICポートアドレスがIntel仕様に準拠した値であること
 * 目的: ハードウェアアクセス時のポートアドレスの正確性を保証する
 */
KFS_TEST(test_pic_port_addresses)
{
	KFS_ASSERT_EQ(0x20, PIC_MASTER_CMD);
	KFS_ASSERT_EQ(0x21, PIC_MASTER_IMR);
	KFS_ASSERT_EQ(0xA0, PIC_SLAVE_CMD);
	KFS_ASSERT_EQ(0xA1, PIC_SLAVE_IMR);
}

/** IRQベクタオフセット定数検証
 * 検証対象: IRQ0_VECTOR, IRQ8_VECTOR
 * 検証項目: IRQ→ベクタ変換オフセットがリマップ後の値であること
 * 目的: 例外(0x00-0x1F)との衝突を避けたマッピングを保証する
 */
KFS_TEST(test_irq_vector_offsets)
{
	KFS_ASSERT_EQ(0x20, IRQ0_VECTOR);
	KFS_ASSERT_EQ(0x28, IRQ8_VECTOR);
}

/** IRQ数定数検証
 * 検証対象: NR_IRQS
 * 検証項目: 8259A PICがサポートするIRQ数が16であること
 * 目的: マスター/スレーブ合計16本のIRQ対応を保証する
 */
KFS_TEST(test_nr_irqs)
{
	KFS_ASSERT_EQ(16, NR_IRQS);
}

/** EOIコマンド定数検証
 * 検証対象: PIC_EOI
 * 検証項目: Non-specific EOIコマンドが0x20であること
 * 目的: 割り込み終了通知のコマンドバイトの正確性を保証する
 */
KFS_TEST(test_pic_eoi_command)
{
	KFS_ASSERT_EQ(0x20, PIC_EOI);
}

/** カスケードIR定数検証
 * 検証対象: PIC_CASCADE_IR
 * 検証項目: スレーブPICがマスターPICのIR2に接続されること
 * 目的: カスケード接続の設定の正確性を保証する
 */
KFS_TEST(test_pic_cascade_ir)
{
	KFS_ASSERT_EQ(2, PIC_CASCADE_IR);
}

/** ICW4デフォルト値検証
 * 検証対象: MASTER_ICW4_DEFAULT, SLAVE_ICW4_DEFAULT
 * 検証項目: ICW4が8086モード(0x01)に設定されること
 * 目的: PIC動作モードの設定値の正確性を保証する
 */
KFS_TEST(test_icw4_defaults)
{
	KFS_ASSERT_EQ(0x01, MASTER_ICW4_DEFAULT);
	KFS_ASSERT_EQ(0x01, SLAVE_ICW4_DEFAULT);
}

static struct kfs_test_case cases[] = {
	/* 定数テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_pic_port_addresses, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_irq_vector_offsets, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_irqs, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pic_eoi_command, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pic_cascade_ir, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_icw4_defaults, setup_test, teardown_test),
};

int register_unit_tests_i8259(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
