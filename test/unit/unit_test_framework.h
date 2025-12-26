#ifndef KFS_UNIT_TEST_FRAMEWORK_H
#define KFS_UNIT_TEST_FRAMEWORK_H

#include <kfs/printk.h>

typedef void (*kfs_test_fn)(void);
typedef void (*kfs_test_init_fn)(void);
typedef void (*kfs_test_exit_fn)(void);

struct kfs_test_case
{
	const char *name;
	kfs_test_fn fn;
	kfs_test_init_fn init; // テスト前に実行（NULLの場合はスキップ）
	kfs_test_exit_fn exit; // テスト後に実行（NULLの場合はスキップ）
};

#define KFS_TEST(name) static void name(void)

#define KFS_ASSERT_EQ(expected, actual)                                                                                \
	do                                                                                                                 \
	{                                                                                                                  \
		long long _e = (long long)(expected);                                                                          \
		long long _a = (long long)(actual);                                                                            \
		if (_e != _a)                                                                                                  \
		{                                                                                                              \
			printk("[FAIL] %s:%d %s == %s  expected=%lld actual=%lld\n", __FILE__, __LINE__, #expected, #actual, _e,   \
				   _a);                                                                                                \
			kfs_test_failures++;                                                                                       \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define KFS_ASSERT_TRUE(expr)                                                                                          \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(expr))                                                                                                   \
		{                                                                                                              \
			printk("[FAIL] %s:%d %s is false\n", __FILE__, __LINE__, #expr);                                           \
			kfs_test_failures++;                                                                                       \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

extern int kfs_test_failures;

// init/exitなしのテスト登録
#define KFS_REGISTER_TEST(fn)                                                                                          \
	{                                                                                                                  \
		#fn, fn, NULL, NULL                                                                                            \
	}

// init/exitありのテスト登録
#define KFS_REGISTER_TEST_WITH_SETUP(fn, init_fn, exit_fn)                                                             \
	{                                                                                                                  \
		#fn, fn, init_fn, exit_fn                                                                                      \
	}

static inline int kfs_run_all_tests(const struct kfs_test_case *cases, int count)
{
	int executed = 0;
	for (int i = 0; i < count; ++i)
	{
		printk("[RUN ] %s\n", cases[i].name);

		// テスト前のセットアップ実行
		if (cases[i].init)
		{
			cases[i].init();
		}

		// テスト本体を実行
		cases[i].fn();

		// テスト後のクリーンアップ実行（失敗時も必ず実行）
		if (cases[i].exit)
		{
			cases[i].exit();
		}

		if (kfs_test_failures == 0)
		{
			printk("[PASS] %s\n", cases[i].name);
		}
		else
		{
			/* failure already reported */
		}
		executed++;
	}
	if (kfs_test_failures)
	{
		printk("== SUMMARY: %d tests, %d failed ==\n", executed, kfs_test_failures);
	}
	else
	{
		printk("== SUMMARY: %d tests, all passed ==\n", executed);
	}
	return kfs_test_failures == 0 ? 0 : 1;
}

#endif // KFS_UNIT_TEST_FRAMEWORK_H
