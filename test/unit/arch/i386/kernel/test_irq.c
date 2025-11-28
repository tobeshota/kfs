#include "../../../test_reset.h"
#include "host_test_framework.h"
#include <asm-i386/ptrace.h>
#include <kfs/irq.h>
#include <kfs/stddef.h>
#include <kfs/stdint.h>

/* テスト用IRQハンドラ呼び出しカウンタ */
static volatile int handler_called_count = 0;
static volatile int last_irq_received = -1;

/* テスト用IRQハンドラ */
static int test_irq_handler(int irq, struct pt_regs *regs)
{
	(void)regs;
	handler_called_count++;
	last_irq_received = irq;
	return IRQ_HANDLED;
}

/* セットアップ */
static void setup_test(void)
{
	reset_all_state_for_test();
	handler_called_count = 0;
	last_irq_received = -1;
}

static void teardown_test(void)
{
	/* 登録されたハンドラをクリア */
	for (unsigned int i = 0; i < NR_IRQS; i++)
	{
		struct irq_desc *desc = irq_to_desc(i);
		if (desc && desc->action)
		{
			free_irq(i, desc->action->dev_id);
		}
	}
}

/* ========== irq_to_desc()テスト ========== */

/** irq_to_desc()有効範囲検証
 * 検証対象: irq_to_desc()
 * 検証項目: 有効なIRQ番号(0-15)でNULL以外を返すこと
 * 目的: 有効なIRQ番号でディスクリプタが取得できることを保証する
 */
KFS_TEST(test_irq_to_desc_valid)
{
	KFS_ASSERT_TRUE(irq_to_desc(0) != NULL);
	KFS_ASSERT_TRUE(irq_to_desc(7) != NULL);
	KFS_ASSERT_TRUE(irq_to_desc(8) != NULL);
	KFS_ASSERT_TRUE(irq_to_desc(15) != NULL);
}

/** irq_to_desc()範囲外検証
 * 検証対象: irq_to_desc()
 * 検証項目: 無効なIRQ番号(16以上)でNULLを返すこと
 * 目的: 範囲外アクセスによるバッファオーバーフローを防止する
 */
KFS_TEST(test_irq_to_desc_invalid)
{
	KFS_ASSERT_TRUE(irq_to_desc(16) == NULL);
	KFS_ASSERT_TRUE(irq_to_desc(100) == NULL);
	KFS_ASSERT_TRUE(irq_to_desc(255) == NULL);
}

/* ========== request_irq()テスト ========== */

/** request_irq()正常登録検証
 * 検証対象: request_irq()
 * 検証項目: 有効なパラメータで0(成功)を返すこと
 * 目的: IRQハンドラが正常に登録できることを保証する
 */
KFS_TEST(test_request_irq_success)
{
	int ret = request_irq(1, test_irq_handler, "test", (void *)0x1234);
	KFS_ASSERT_EQ(0, ret);

	/* ハンドラが登録されていることを確認 */
	struct irq_desc *desc = irq_to_desc(1);
	KFS_ASSERT_TRUE(desc != NULL);
	KFS_ASSERT_TRUE(desc->action != NULL);
	KFS_ASSERT_TRUE(desc->action->handler == test_irq_handler);
}

/** request_irq()無効IRQ番号検証
 * 検証対象: request_irq()
 * 検証項目: 無効なIRQ番号で-1(エラー)を返すこと
 * 目的: 範囲外IRQの登録を拒否してシステムの安定性を保証する
 */
KFS_TEST(test_request_irq_invalid_irq)
{
	int ret = request_irq(16, test_irq_handler, "test", NULL);
	KFS_ASSERT_EQ(-1, ret);

	ret = request_irq(100, test_irq_handler, "test", NULL);
	KFS_ASSERT_EQ(-1, ret);
}

/** request_irq()NULLハンドラ検証
 * 検証対象: request_irq()
 * 検証項目: NULLハンドラで-1(エラー)を返すこと
 * 目的: 無効なハンドラの登録を防止する
 */
KFS_TEST(test_request_irq_null_handler)
{
	int ret = request_irq(1, NULL, "test", NULL);
	KFS_ASSERT_EQ(-1, ret);
}

/** request_irq()二重登録検証
 * 検証対象: request_irq()
 * 検証項目: 既に登録済みのIRQに対して-1(エラー)を返すこと
 * 目的: ハンドラの上書きを防止し、明示的なfree_irq()を要求する
 */
KFS_TEST(test_request_irq_already_registered)
{
	int ret = request_irq(2, test_irq_handler, "test1", (void *)1);
	KFS_ASSERT_EQ(0, ret);

	/* 同じIRQに再度登録しようとするとエラー */
	ret = request_irq(2, test_irq_handler, "test2", (void *)2);
	KFS_ASSERT_EQ(-1, ret);
}

/* ========== free_irq()テスト ========== */

/** free_irq()正常解除検証
 * 検証対象: free_irq()
 * 検証項目: 登録されたハンドラが解除され、actionがNULLになること
 * 目的: IRQハンドラが正常に解除できることを保証する
 */
KFS_TEST(test_free_irq_success)
{
	void *dev_id = (void *)0x5678;
	request_irq(3, test_irq_handler, "test", dev_id);

	free_irq(3, dev_id);

	/* ハンドラが解除されていることを確認 */
	struct irq_desc *desc = irq_to_desc(3);
	KFS_ASSERT_TRUE(desc != NULL);
	KFS_ASSERT_TRUE(desc->action == NULL);
}

/** free_irq()dev_id不一致検証
 * 検証対象: free_irq()
 * 検証項目: dev_idが一致しない場合はハンドラが解除されないこと
 * 目的: 誤ったdev_idによる意図しない解除を防止する
 */
KFS_TEST(test_free_irq_wrong_dev_id)
{
	void *dev_id = (void *)0xABCD;
	request_irq(4, test_irq_handler, "test", dev_id);

	/* 異なるdev_idで解除しようとしても解除されない */
	free_irq(4, (void *)0x1111);

	struct irq_desc *desc = irq_to_desc(4);
	KFS_ASSERT_TRUE(desc->action != NULL);
}

/* ========== do_IRQ()テスト ========== */

/** do_IRQ()ハンドラ呼び出し検証
 * 検証対象: do_IRQ()
 * 検証項目: 登録されたハンドラが呼び出されること
 * 目的: IRQ発生時に正しいハンドラが実行されることを保証する
 */
KFS_TEST(test_do_IRQ_calls_handler)
{
	request_irq(5, test_irq_handler, "test", NULL);

	struct pt_regs regs = {0};
	regs.orig_eax = 5; /* IRQ番号 */

	do_IRQ(&regs);

	KFS_ASSERT_EQ(1, handler_called_count);
	KFS_ASSERT_EQ(5, last_irq_received);
}

/** do_IRQ()未登録IRQ検証
 * 検証対象: do_IRQ()
 * 検証項目: ハンドラ未登録のIRQでもクラッシュしないこと
 * 目的: 予期しないIRQに対するシステムの堅牢性を保証する
 */
KFS_TEST(test_do_IRQ_no_handler)
{
	struct pt_regs regs = {0};
	regs.orig_eax = 6; /* ハンドラ未登録のIRQ */

	do_IRQ(&regs); /* クラッシュしなければOK */

	KFS_ASSERT_EQ(0, handler_called_count);
}

/** do_IRQ()割り込みカウント検証
 * 検証対象: do_IRQ()
 * 検証項目: 割り込み発生回数がカウントされること
 * 目的: 割り込み統計情報が正しく収集されることを保証する
 */
KFS_TEST(test_do_IRQ_increments_count)
{
	request_irq(7, test_irq_handler, "test", NULL);

	struct irq_desc *desc = irq_to_desc(7);
	unsigned int initial_count = desc->count;

	struct pt_regs regs = {0};
	regs.orig_eax = 7;

	do_IRQ(&regs);
	KFS_ASSERT_EQ(initial_count + 1, desc->count);

	do_IRQ(&regs);
	KFS_ASSERT_EQ(initial_count + 2, desc->count);
}

/** do_IRQ()範囲外IRQ検証
 * 検証対象: do_IRQ()
 * 検証項目: 範囲外IRQ番号でもクラッシュしないこと
 * 目的: 不正なIRQ番号に対するシステムの堅牢性を保証する
 */
KFS_TEST(test_do_IRQ_invalid_irq)
{
	struct pt_regs regs = {0};
	regs.orig_eax = 20; /* 範囲外のIRQ */

	do_IRQ(&regs); /* クラッシュしなければOK */

	KFS_ASSERT_TRUE(1);
}

static struct kfs_test_case cases[] = {
	/* irq_to_desc()テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_irq_to_desc_valid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_irq_to_desc_invalid, setup_test, teardown_test),
	/* request_irq()テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_request_irq_success, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_request_irq_invalid_irq, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_request_irq_null_handler, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_request_irq_already_registered, setup_test, teardown_test),
	/* free_irq()テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_free_irq_success, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_free_irq_wrong_dev_id, setup_test, teardown_test),
	/* do_IRQ()テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_do_IRQ_calls_handler, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_IRQ_no_handler, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_IRQ_increments_count, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_IRQ_invalid_irq, setup_test, teardown_test),
};

int register_host_tests_irq(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
