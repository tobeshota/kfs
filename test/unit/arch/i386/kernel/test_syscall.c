/**
 * test_syscall.c - syscall.cのユニットテスト
 *
 * システムコールディスパッチャのテスト
 */
#include "../../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/stdint.h>
#include <kfs/syscall.h>

/* セットアップ */
static void setup_test(void)
{
	reset_all_state_for_test();
}

static void teardown_test(void)
{
}

/* ========== do_syscall テスト ========== */

/**
 * 無効なシステムコール番号（負の値）の検証
 * 検証対象: do_syscall()
 * 検証項目: 負のシステムコール番号で-ENOSYS (-38)を返すこと
 * 目的: 境界チェックの正確性を保証する
 */
KFS_TEST(test_do_syscall_negative_nr)
{
	long result = do_syscall(-1, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 無効なシステムコール番号（大きすぎる値）の検証
 * 検証対象: do_syscall()
 * 検証項目: NR_syscalls以上の番号で-ENOSYS (-38)を返すこと
 * 目的: sys_call_tableの境界外アクセスを防止する
 */
KFS_TEST(test_do_syscall_too_large_nr)
{
	long result = do_syscall(NR_syscalls, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 境界値テスト（NR_syscalls + 1）
 * 検証対象: do_syscall()
 * 検証項目: NR_syscalls + 1でも-ENOSYSを返すこと
 * 目的: オフバイワンエラーがないことを確認
 */
KFS_TEST(test_do_syscall_boundary_plus_one)
{
	long result = do_syscall(NR_syscalls + 1, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 未実装syscall（番号0）の検証
 * 検証対象: do_syscall()
 * 検証項目: syscall 0は未実装なので-ENOSYSを返すこと
 * 目的: sys_ni_syscallがデフォルトハンドラとして機能することを確認
 */
KFS_TEST(test_do_syscall_unimplemented_0)
{
	long result = do_syscall(0, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 未実装syscall（__NR_exit）の検証
 * 検証対象: do_syscall()
 * 検証項目: __NR_exit (1)は定義されているが未実装なので-ENOSYSを返すこと
 * 目的: syscall番号が定義されていても実装がなければ-ENOSYSを返すことを確認
 */
KFS_TEST(test_do_syscall_unimplemented_exit)
{
	long result = do_syscall(__NR_exit, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 未実装syscall（__NR_write）の検証
 * 検証対象: do_syscall()
 * 検証項目: __NR_write (4)は定義されているが未実装なので-ENOSYSを返すこと
 * 目的: syscall番号が定義されていても実装がなければ-ENOSYSを返すことを確認
 */
KFS_TEST(test_do_syscall_unimplemented_write)
{
	long result = do_syscall(__NR_write, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 有効範囲の最大値テスト
 * 検証対象: do_syscall()
 * 検証項目: NR_syscalls - 1 (最後の有効な番号)が正しく処理されること
 * 目的: 有効範囲の境界で正常動作することを確認
 */
KFS_TEST(test_do_syscall_max_valid_nr)
{
	long result = do_syscall(NR_syscalls - 1, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * 大きな負の値のテスト
 * 検証対象: do_syscall()
 * 検証項目: 大きな負の値でも正しく-ENOSYSを返すこと
 * 目的: オーバーフロー等の問題がないことを確認
 */
KFS_TEST(test_do_syscall_large_negative)
{
	long result = do_syscall(-1000000, 0, 0, 0, 0, 0);
	KFS_ASSERT_EQ(-ENOSYS, result);
}

/**
 * NR_syscalls定数の検証
 * 検証対象: NR_syscalls
 * 検証項目: NR_syscallsが8であること
 * 目的: syscall.hの定義とsyscall.cの整合性を確認
 */
KFS_TEST(test_nr_syscalls_value)
{
	KFS_ASSERT_EQ(8, NR_syscalls);
}

/**
 * __NR_exit定数の検証
 * 検証対象: __NR_exit
 * 検証項目: __NR_exitが1であること（Linux互換）
 * 目的: Linux 2.6.11との互換性を確認
 */
KFS_TEST(test_nr_exit_value)
{
	KFS_ASSERT_EQ(1, __NR_exit);
}

/**
 * __NR_write定数の検証
 * 検証対象: __NR_write
 * 検証項目: __NR_writeが4であること（Linux互換）
 * 目的: Linux 2.6.11との互換性を確認
 */
KFS_TEST(test_nr_write_value)
{
	KFS_ASSERT_EQ(4, __NR_write);
}

/**
 * INT 0x80による無効なシステムコール番号テスト
 * 検証対象: entry.S の system_call + do_syscall の境界チェック
 * 検証項目: 無効なシステムコール番号(999)で-ENOSYSが返ること
 * 目的: 範囲外のシステムコール番号が正しく処理されることを確認
 */
KFS_TEST(test_int80_invalid_syscall_nr)
{
	long result;
	__asm__ volatile("movl $999, %%eax\n\t" /* 無効なシステムコール番号 */
					 "int $0x80\n\t"		/* システムコール発行 */
					 "movl %%eax, %0"		/* 戻り値を取得 */
					 : "=r"(result)
					 :
					 : "eax");
	KFS_ASSERT_EQ(-ENOSYS, result);
}

static struct kfs_test_case cases[] = {
	/* do_syscall境界チェックテスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_negative_nr, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_too_large_nr, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_boundary_plus_one, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_large_negative, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_max_valid_nr, setup_test, teardown_test),
	/* 未実装syscallテスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_unimplemented_0, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_unimplemented_exit, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_unimplemented_write, setup_test, teardown_test),
	/* 定数検証テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_syscalls_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_exit_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_write_value, setup_test, teardown_test),
	/* INT 0x80テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_int80_invalid_syscall_nr, setup_test, teardown_test),
};

int register_unit_tests_syscall(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
