#include "../../../test_reset.h"
#include "unit_test_framework.h"
#include <asm-i386/desc.h>
#include <asm-i386/ptrace.h>
#include <kfs/stdint.h>
#include <kfs/string.h>

/* traps.cで定義された関数 */
extern struct idt_entry *idt_get_entry(int n);
extern void show_regs(struct pt_regs *regs);
extern void do_exception(struct pt_regs *regs);

/* テスト用IDTテーブル */
static struct idt_entry test_idt[IDT_ENTRIES];

/* ダミーハンドラ */
static void dummy_handler(void)
{
}

/* セットアップ */
static void setup_test(void)
{
	reset_all_state_for_test();
	memset(test_idt, 0, sizeof(test_idt));
}

static void teardown_test(void)
{
}

/* ========== IDT管理テスト ========== */

/** IDTエントリサイズ検証
 * 検証対象: struct idt_entry
 * 検証項目: IDTエントリ構造体のサイズが8バイトであること
 * 目的: Intel i386仕様に準拠したIDTエントリサイズを保証する
 */
KFS_TEST(test_idt_entry_size)
{
	KFS_ASSERT_EQ(8, sizeof(struct idt_entry));
}

/** ゲート設定時のハンドラアドレス検証
 * 検証対象: _set_gate()マクロ
 * 検証項目: ハンドラアドレスがbase_lo/base_hiに正しく分割・格納されること
 * 目的: 割り込み発生時に正しいハンドラが呼び出されることを保証する
 */
KFS_TEST(test_set_gate_address)
{
	uint32_t handler_addr = (uint32_t)dummy_handler;

	_set_gate(test_idt, 0, dummy_handler, IDT_GATE_INTERRUPT);

	uint32_t stored_addr = test_idt[0].base_lo | ((uint32_t)test_idt[0].base_hi << 16);
	KFS_ASSERT_EQ(handler_addr, stored_addr);
}

/** ゲート設定時のセグメントセレクタ検証
 * 検証対象: _set_gate()マクロ
 * 検証項目: セレクタフィールドに__KERNEL_CSが設定されること
 * 目的: 割り込みハンドラがカーネルコードセグメントで実行されることを保証する
 */
KFS_TEST(test_set_gate_selector)
{
	_set_gate(test_idt, 1, dummy_handler, IDT_GATE_INTERRUPT);

	KFS_ASSERT_EQ(__KERNEL_CS, test_idt[1].selector);
}

/** ゲート設定時のフラグ検証
 * 検証対象: _set_gate()マクロ
 * 検証項目: 各ゲートタイプ(INTERRUPT/TRAP/USER)のフラグが正しく設定されること
 * 目的: ゲートタイプとDPLが正しく設定され、適切な特権レベルで動作することを保証する
 */
KFS_TEST(test_set_gate_flags)
{
	_set_gate(test_idt, 2, dummy_handler, IDT_GATE_INTERRUPT);
	KFS_ASSERT_EQ(IDT_GATE_INTERRUPT, test_idt[2].flags);

	_set_gate(test_idt, 3, dummy_handler, IDT_GATE_TRAP);
	KFS_ASSERT_EQ(IDT_GATE_TRAP, test_idt[3].flags);

	_set_gate(test_idt, 4, dummy_handler, IDT_GATE_USER);
	KFS_ASSERT_EQ(IDT_GATE_USER, test_idt[4].flags);
}

/** ゲート設定時の予約フィールド検証
 * 検証対象: _set_gate()マクロ
 * 検証項目: zeroフィールドが常に0に設定されること
 * 目的: Intel仕様で予約されているフィールドが正しく初期化されることを保証する
 */
KFS_TEST(test_set_gate_zero_field)
{
	_set_gate(test_idt, 5, dummy_handler, IDT_GATE_INTERRUPT);

	KFS_ASSERT_EQ(0, test_idt[5].zero);
}

/** IDTエントリ取得時の境界チェック検証
 * 検証対象: idt_get_entry()
 * 検証項目: 範囲外インデックスでNULLを返し、有効範囲では非NULLを返すこと
 * 目的: 不正なインデックスによるメモリ破壊を防止する
 */
KFS_TEST(test_idt_get_entry_bounds)
{
	KFS_ASSERT_TRUE(idt_get_entry(-1) == NULL);
	KFS_ASSERT_TRUE(idt_get_entry(256) == NULL);
	KFS_ASSERT_TRUE(idt_get_entry(0) != NULL);
	KFS_ASSERT_TRUE(idt_get_entry(255) != NULL);
}

/** IDTゲートタイプ定数検証
 * 検証対象: IDT_GATE_INTERRUPT, IDT_GATE_TRAP, IDT_GATE_USER
 * 検証項目: 各定数がIntel仕様に準拠した値であること
 * 目的: ゲートタイプ定数がハードウェア仕様と一致することを保証する
 */
KFS_TEST(test_idt_gate_constants)
{
	KFS_ASSERT_EQ(0x8E, IDT_GATE_INTERRUPT);
	KFS_ASSERT_EQ(0x8F, IDT_GATE_TRAP);
	KFS_ASSERT_EQ(0xEE, IDT_GATE_USER);
}

/** IDTエントリ数検証
 * 検証対象: IDT_ENTRIES
 * 検証項目: IDTエントリ数が256であること
 * 目的: i386アーキテクチャで定義された割り込みベクタ数と一致することを保証する
 */
KFS_TEST(test_idt_entries_count)
{
	KFS_ASSERT_EQ(256, IDT_ENTRIES);
}

/* ========== 例外ハンドラテスト ========== */

/** pt_regs構造体サイズ検証
 * 検証対象: struct pt_regs
 * 検証項目: 構造体サイズが68バイト(17レジスタ×4バイト)であること
 * 目的: entry.SのSAVE_REGSで保存されるレジスタと構造体が一致することを保証する
 */
KFS_TEST(test_pt_regs_size)
{
	KFS_ASSERT_EQ(68, sizeof(struct pt_regs));
}

/** pt_regs構造体メンバオフセット検証
 * 検証対象: struct pt_regs
 * 検証項目: 各メンバのオフセットがentry.Sのスタックレイアウトと一致すること
 * 目的: アセンブリコードとC構造体の整合性を保証する
 */
KFS_TEST(test_pt_regs_member_offsets)
{
	KFS_ASSERT_EQ(0, __builtin_offsetof(struct pt_regs, ebx));
	KFS_ASSERT_EQ(4, __builtin_offsetof(struct pt_regs, ecx));
	KFS_ASSERT_EQ(8, __builtin_offsetof(struct pt_regs, edx));
	KFS_ASSERT_EQ(12, __builtin_offsetof(struct pt_regs, esi));
	KFS_ASSERT_EQ(16, __builtin_offsetof(struct pt_regs, edi));
	KFS_ASSERT_EQ(20, __builtin_offsetof(struct pt_regs, ebp));
	KFS_ASSERT_EQ(24, __builtin_offsetof(struct pt_regs, eax));
	KFS_ASSERT_EQ(28, __builtin_offsetof(struct pt_regs, ds));
	KFS_ASSERT_EQ(32, __builtin_offsetof(struct pt_regs, es));
	KFS_ASSERT_EQ(36, __builtin_offsetof(struct pt_regs, fs));
	KFS_ASSERT_EQ(40, __builtin_offsetof(struct pt_regs, gs));
	KFS_ASSERT_EQ(44, __builtin_offsetof(struct pt_regs, orig_eax));
	KFS_ASSERT_EQ(48, __builtin_offsetof(struct pt_regs, eip));
	KFS_ASSERT_EQ(52, __builtin_offsetof(struct pt_regs, cs));
	KFS_ASSERT_EQ(56, __builtin_offsetof(struct pt_regs, eflags));
	KFS_ASSERT_EQ(60, __builtin_offsetof(struct pt_regs, esp));
	KFS_ASSERT_EQ(64, __builtin_offsetof(struct pt_regs, ss));
}

/** show_regs()安定性検証
 * 検証対象: show_regs()
 * 検証項目: 任意のレジスタ値を渡してもクラッシュしないこと
 * 目的: デバッグ出力関数の堅牢性を保証する
 */
KFS_TEST(test_show_regs_no_crash)
{
	struct pt_regs regs = {
		.eax = 0x11111111,
		.ebx = 0x22222222,
		.ecx = 0x33333333,
		.edx = 0x44444444,
		.esi = 0x55555555,
		.edi = 0x66666666,
		.ebp = 0x77777777,
		.esp = 0x88888888,
		.eip = 0x99999999,
		.eflags = 0x00000202,
		.cs = 0x08,
		.ds = 0x10,
		.es = 0x10,
		.fs = 0x10,
		.gs = 0x10,
		.ss = 0x10,
		.orig_eax = 0,
	};

	show_regs(&regs);
	KFS_ASSERT_TRUE(1);
}

/** ブレークポイント例外継続検証
 * 検証対象: do_exception()
 * 検証項目: 例外番号3(Breakpoint)で例外ハンドラがパニックせず正常復帰すること
 * 目的: デバッグ用ブレークポイントからの継続実行を保証する
 */
KFS_TEST(test_do_exception_breakpoint_continues)
{
	struct pt_regs regs = {
		.eax = 0,
		.ebx = 0,
		.ecx = 0,
		.edx = 0,
		.esi = 0,
		.edi = 0,
		.ebp = 0,
		.esp = 0xFFFFFFFF,
		.eip = 0x12345678,
		.eflags = 0x202,
		.cs = 0x08,
		.ds = 0x10,
		.es = 0x10,
		.fs = 0x10,
		.gs = 0x10,
		.ss = 0x10,
		.orig_eax = 3, /* Breakpoint */
	};

	do_exception(&regs);
	KFS_ASSERT_TRUE(1);
}

/** オーバーフロー例外継続検証
 * 検証対象: do_exception()
 * 検証項目: 例外番号4(Overflow)で例外ハンドラがパニックせず正常復帰すること
 * 目的: INTO命令によるオーバーフロー検出からの継続実行を保証する
 */
KFS_TEST(test_do_exception_overflow_continues)
{
	struct pt_regs regs = {
		.eax = 0,
		.ebx = 0,
		.ecx = 0,
		.edx = 0,
		.esi = 0,
		.edi = 0,
		.ebp = 0,
		.esp = 0xFFFFFFFF,
		.eip = 0x12345678,
		.eflags = 0x202,
		.cs = 0x08,
		.ds = 0x10,
		.es = 0x10,
		.fs = 0x10,
		.gs = 0x10,
		.ss = 0x10,
		.orig_eax = 4, /* Overflow */
	};

	do_exception(&regs);
	KFS_ASSERT_TRUE(1);
}

static struct kfs_test_case cases[] = {
	/* IDT管理テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_idt_entry_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_set_gate_address, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_set_gate_selector, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_set_gate_flags, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_set_gate_zero_field, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_idt_get_entry_bounds, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_idt_gate_constants, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_idt_entries_count, setup_test, teardown_test),
	/* 例外ハンドラテスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_pt_regs_size, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pt_regs_member_offsets, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_show_regs_no_crash, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exception_breakpoint_continues, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_exception_overflow_continues, setup_test, teardown_test),
};

int register_unit_tests_traps(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
