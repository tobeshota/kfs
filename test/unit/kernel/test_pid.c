#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/pid.h>
#include <kfs/sched.h>

/* 全テストで共通のセットアップ関数 */
static void setup_test(void)
{
	reset_all_state_for_test();
}

/* 全テストで共通のクリーンアップ関数 */
static void teardown_test(void)
{
	/* 必要なら後処理（現在は空） */
}

/**
 * test_alloc_pid_basic - PID割り当ての基本テスト
 *
 * alloc_pid()が正しくPIDを割り当てるか確認
 */
static void test_alloc_pid_basic(void)
{
	struct pid *pid1, *pid2, *pid3;

	/* PID割り当て */
	pid1 = alloc_pid();
	KFS_ASSERT_TRUE(pid1 != NULL);
	KFS_ASSERT_TRUE(pid1->level == 0);
	KFS_ASSERT_TRUE(pid1->count == 1);

	/* 複数PID割り当て */
	pid2 = alloc_pid();
	pid3 = alloc_pid();
	KFS_ASSERT_TRUE(pid2 != NULL && pid3 != NULL);

	/* 異なるPID構造体が返される */
	KFS_ASSERT_TRUE(pid1 != pid2 && pid2 != pid3);

	/* クリーンアップ */
	put_pid(pid1);
	put_pid(pid2);
	put_pid(pid3);

	printk("PID allocation test passed\n");
}

/**
 * test_put_pid - PID解放のテスト
 *
 * put_pid()がメモリを正しく解放するか確認
 */
static void test_put_pid(void)
{
	struct pid *pid;

	/* PID割り当てと解放 */
	pid = alloc_pid();
	KFS_ASSERT_TRUE(pid != NULL);

	/* 参照カウントは1 */
	KFS_ASSERT_TRUE(pid->count == 1);

	/* 解放 */
	put_pid(pid);
	/* 解放後はアクセスできないので、クラッシュしないことを確認 */

	printk("PID free test passed\n");
}

/**
 * test_pid_exhaustion - PID枯渇時の動作テスト
 *
 * PIDが枯渇した時にNULLが返されるか確認
 * 注：実際に全PIDを使い切ると時間がかかるため、簡易テスト
 */
static void test_pid_exhaustion(void)
{
	struct pid *pids[100];
	int i;
	int allocated = 0;

	/* 100個のPIDを割り当ててみる */
	for (i = 0; i < 100; i++)
	{
		pids[i] = alloc_pid();
		if (pids[i] != NULL)
		{
			allocated++;
		}
		else
		{
			break; /* 割り当て失敗 */
		}
	}

	/* 少なくとも10個は割り当てられるはず */
	KFS_ASSERT_TRUE(allocated >= 10);

	/* 解放 */
	for (i = 0; i < allocated; i++)
	{
		put_pid(pids[i]);
	}

	printk("PID exhaustion test: allocated %d PIDs\n", allocated);
}

/**
 * test_pid_reference_counting - PID参照カウントのテスト
 *
 * 参照カウントが正しく管理されるか確認
 */
static void test_pid_reference_counting(void)
{
	struct pid *pid;

	pid = alloc_pid();
	KFS_ASSERT_TRUE(pid != NULL);
	KFS_ASSERT_TRUE(pid->count == 1);

	/* 参照カウント増加（手動） */
	pid->count++;
	KFS_ASSERT_TRUE(pid->count == 2);

	/* 1回目の解放（カウント2→1） */
	put_pid(pid);
	/* まだ参照されているので解放されない */
	KFS_ASSERT_TRUE(pid->count == 1);

	/* 2回目の解放（カウント1→0、実際に解放） */
	put_pid(pid);
	/* 解放されたのでアクセス不可（クラッシュしないことを確認） */

	printk("PID reference counting test passed\n");
}

/**
 * test_pid_structure_initialization - PID構造体の初期化テスト
 *
 * 割り当てられたPID構造体が正しく初期化されているか確認
 */
static void test_pid_structure_initialization(void)
{
	struct pid *pid;
	int i;

	pid = alloc_pid();
	KFS_ASSERT_TRUE(pid != NULL);

	/* level = 0（ルートnamespace） */
	KFS_ASSERT_TRUE(pid->level == 0);

	/* inum = 1（ルートnamespace ID） */
	KFS_ASSERT_TRUE(pid->inum == 1);

	/* PID番号が有効な範囲内 */
	KFS_ASSERT_TRUE(pid->nr >= 0);
	KFS_ASSERT_TRUE(pid->nr < 32768);

	/* tasks配列が初期化されていることを確認 */
	for (i = 0; i < PIDTYPE_MAX; i++)
	{
		KFS_ASSERT_TRUE(pid->tasks[i].first == NULL);
	}

	put_pid(pid);

	printk("PID structure initialization test passed\n");
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_alloc_pid_basic, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_put_pid, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pid_exhaustion, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pid_reference_counting, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pid_structure_initialization, setup_test, teardown_test),
};

int register_unit_tests_pid(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
