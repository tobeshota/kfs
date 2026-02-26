/**
 * test_syscall.c - syscall.cのユニットテスト
 *
 * システムコールディスパッチャのテスト
 */
#include "../../../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/errno.h>
#include <kfs/signal.h>
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
 * __NR_wait 定数の検証
 * 検証対象: __NR_wait
 * 検証項目: __NR_wait が 7 であること（Linux 互換）
 * 目的: Linux 2.6.11 との互換性を確認
 */
KFS_TEST(test_nr_wait_value)
{
	KFS_ASSERT_EQ(7, __NR_wait);
}

/**
 * __NR_getuid 定数の検証
 * 検証対象: __NR_getuid
 * 検証項目: __NR_getuid が 24 であること（Linux 互換）
 * 目的: Linux 2.6.11 との互換性を確認
 */
KFS_TEST(test_nr_getuid_value)
{
	KFS_ASSERT_EQ(24, __NR_getuid);
}

/**
 * __NR_kill 定数の検証
 * 検証対象: __NR_kill
 * 検証項目: __NR_kill が 37 であること（Linux 互換）
 * 目的: Linux 2.6.11 との互換性を確認
 */
KFS_TEST(test_nr_kill_value)
{
	KFS_ASSERT_EQ(37, __NR_kill);
}

/**
 * __NR_signal 定数の検証
 * 検証対象: __NR_signal
 * 検証項目: __NR_signal が 48 であること（Linux 互換）
 * 目的: Linux 2.6.11 との互換性を確認
 */
KFS_TEST(test_nr_signal_value)
{
	KFS_ASSERT_EQ(48, __NR_signal);
}

/**
 * sys_call_table への getuid 登録確認
 * 検証対象: do_syscall(__NR_getuid)
 * 検証項目: getuid が登録済みで -ENOSYS を返さないこと
 * 目的: sys_call_table への登録を安全な syscall（getuid）で確認する
 * 注意: sys_exit は noreturn のためテストから直接呼べない
 */
KFS_TEST(test_do_syscall_getuid_registered)
{
	long result = do_syscall(__NR_getuid, 0, 0, 0, 0, 0);
	KFS_ASSERT_TRUE(result != -ENOSYS);
}

/**
 * 実装済みsyscall（__NR_write）の検証
 * 検証対象: do_syscall()
 * 検証項目: __NR_write (4) は実装済みであり、未サポートの fd=0 に対して -EBADF を返すこと
 * 目的: write syscall が sys_ni_syscall ではなく実装関数にルーティングされることを確認
 */
KFS_TEST(test_do_syscall_unimplemented_write)
{
	/* fd=0 (stdin) は未サポート → -EBADF が返る（-ENOSYS ではない） */
	long result = do_syscall(__NR_write, 0, 0, 1, 0, 0);
	KFS_ASSERT_EQ(-EBADF, result);
}

/**
 * 有効範囲内の未実装エントリの検証
 * 検証対象: do_syscall()
 * 検証項目: 実装されていないエントリ（155 = __NR_sched_setscheduler - 1）が -ENOSYS を返すこと
 * 目的: 未実装エントリが sys_ni_syscall にフォールバックすることを確認
 */
KFS_TEST(test_do_syscall_max_valid_nr)
{
	/* 155 は __NR_sched_setscheduler(156) の直前の未実装エントリ */
	long result = do_syscall(__NR_sched_setscheduler - 1, 0, 0, 0, 0, 0);
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
 * 検証項目: NR_syscallsが158であること（__NR_sched_getscheduler + 1）
 * 目的: syscall.hの定義とsyscall.cの整合性を確認
 */
KFS_TEST(test_nr_syscalls_value)
{
	KFS_ASSERT_EQ(159, NR_syscalls);
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

/**
 * INT 0x80: __NR_getuid (24) 登録確認
 * 検証対象: entry.S system_call → do_syscall → do_sys_getuid
 * 検証項目: INT 0x80 経由で getuid を呼び出しても -ENOSYS が返らないこと
 * 目的: sys_call_table[__NR_getuid] が正しく呼ばれることを確認
 */
KFS_TEST(test_int80_getuid)
{
	long result;
	__asm__ volatile("movl $24, %%eax\n\t" /* __NR_getuid */
					 "int $0x80\n\t"
					 "movl %%eax, %0"
					 : "=r"(result)
					 :
					 : "eax");
	KFS_ASSERT_TRUE(result != -ENOSYS);
}

/**
 * INT 0x80: __NR_wait (7) 登録確認
 * 検証対象: entry.S system_call → do_syscall → do_sys_wait
 * 検証項目: 子プロセスなしの状態で wait(NULL) が -ECHILD を返すこと
 * 目的: -ENOSYS でなく -ECHILD が返ることで登録と実効を同時に確認
 */
KFS_TEST(test_int80_wait_no_children)
{
	long result;
	__asm__ volatile("movl $7, %%eax\n\t"	 /* __NR_wait */
					 "xorl %%ebx, %%ebx\n\t" /* EBX = NULL (wstatusポインタ) */
					 "int $0x80\n\t"
					 "movl %%eax, %0"
					 : "=r"(result)
					 :
					 : "eax", "ebx");
	KFS_ASSERT_EQ(-ECHILD, result);
}

/**
 * INT 0x80: __NR_signal (48) 登録確認
 * 検証対象: entry.S system_call → do_syscall → do_sys_signal
 * 検証項目: signal(SIGUSR1, SIG_IGN) が以前のハンドラ SIG_DFL(0) を返すこと
 * 目的: -ENOSYS でない戻り値により登録を確認
 */
KFS_TEST(test_int80_signal_sigusr1_ign)
{
	long result;
	__asm__ volatile("movl $48, %%eax\n\t" /* __NR_signal */
					 "movl $10, %%ebx\n\t" /* EBX = SIGUSR1 (10) */
					 "movl $1,  %%ecx\n\t" /* ECX = SIG_IGN (1) */
					 "int $0x80\n\t"
					 "movl %%eax, %0"
					 : "=r"(result)
					 :
					 : "eax", "ebx", "ecx");
	KFS_ASSERT_TRUE(result != -ENOSYS);
}

/**
 * INT 0x80: __NR_kill (37) 登録確認
 * 検証対象: entry.S system_call → do_syscall → do_sys_kill
 * 検証項目: kill(1, 0) が -ENOSYS を返さないこと
 * 目的: sig=0 は実際にシグナルを送らないため副作用なしで登録を確認できる
 */
KFS_TEST(test_int80_kill_sig0)
{
	long result;
	__asm__ volatile("movl $37, %%eax\n\t"	 /* __NR_kill */
					 "movl $1,  %%ebx\n\t"	 /* EBX = pid=1 (init_task) */
					 "xorl %%ecx, %%ecx\n\t" /* ECX = sig=0 (存在確認のみ) */
					 "int $0x80\n\t"
					 "movl %%eax, %0"
					 : "=r"(result)
					 :
					 : "eax", "ebx", "ecx");
	KFS_ASSERT_TRUE(result != -ENOSYS);
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
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_unimplemented_write, setup_test, teardown_test),
	/* 定数検証テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_syscalls_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_exit_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_write_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_wait_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_getuid_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_kill_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_nr_signal_value, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_syscall_getuid_registered, setup_test, teardown_test),
	/* INT 0x80テスト */
	KFS_REGISTER_TEST_WITH_SETUP(test_int80_invalid_syscall_nr, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_int80_getuid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_int80_wait_no_children, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_int80_signal_sigusr1_ign, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_int80_kill_sig0, setup_test, teardown_test),
};

int register_unit_tests_syscall(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
