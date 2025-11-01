#ifndef KFS_HOST_TEST_FRAMEWORK_H
#define KFS_HOST_TEST_FRAMEWORK_H

#include <setjmp.h> // for sigsetjmp, siglongjmp
#include <signal.h> // for sigaction, SIGSEGV

typedef void (*kfs_test_fn)(void);

struct kfs_test_case
{
	const char *name;
	kfs_test_fn fn;
};

#define KFS_TEST(name) static void name(void)

#define KFS_ASSERT_EQ(expected, actual)                                                                                \
	do                                                                                                                 \
	{                                                                                                                  \
		long long _e = (long long)(expected);                                                                          \
		long long _a = (long long)(actual);                                                                            \
		if (_e != _a)                                                                                                  \
		{                                                                                                              \
			__builtin_printf("[FAIL] %s:%d %s == %s  expected=%lld actual=%lld\n", __FILE__, __LINE__, #expected,      \
							 #actual, _e, _a);                                                                         \
			kfs_test_failures++;                                                                                       \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define KFS_ASSERT_TRUE(expr)                                                                                          \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(expr))                                                                                                   \
		{                                                                                                              \
			__builtin_printf("[FAIL] %s:%d %s is false\n", __FILE__, __LINE__, #expr);                                 \
			kfs_test_failures++;                                                                                       \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

extern int kfs_test_failures;

#define KFS_REGISTER_TEST(fn)                                                                                          \
	{                                                                                                                  \
#fn, fn                                                                                                        \
	}

static sigjmp_buf __KFS_EXPECT_SEGV_env;

static void __KFS_EXPECT_SEGV_handler(int sig)
{
	if (sig == SIGSEGV)
	{
		siglongjmp(__KFS_EXPECT_SEGV_env, 1);
	}
}

#define KFS_EXPECT_SEGV(fn, ...)                                                                                              \
	do                                                                                                                 \
	{                                                                                                                  \
		struct sigaction sa, old_sa;                                                                                   \
		sa.sa_handler = __KFS_EXPECT_SEGV_handler;                                                                            \
		sigemptyset(&sa.sa_mask);                                                                                      \
		sa.sa_flags = 0;                                                                                               \
		sigaction(SIGSEGV, &sa, &old_sa);                                                                              \
                                                                                                                       \
		if (sigsetjmp(__KFS_EXPECT_SEGV_env, 1) == 0)                                                                         \
		{                                                                                                              \
			/* 関数実行（SIGSEGV発生を期待） */                                                             \
			fn(__VA_ARGS__);                                                                                           \
			__builtin_printf("[FAIL] %s:%d expected SIGSEGV, but none occurred\n", __FILE__, __LINE__);                \
			kfs_test_failures++;                                                                                       \
			sigaction(SIGSEGV, &old_sa, 0);                                                                            \
			return;                                                                                                    \
		}                                                                                                              \
		else                                                                                                           \
		{                                                                                                              \
			/* SIGSEGV を検出 */                                                                                    \
			__builtin_printf("[OK  ] %s caused SIGSEGV as expected\n", #fn);                                           \
		}                                                                                                              \
                                                                                                                       \
		sigaction(SIGSEGV, &old_sa, 0);                                                                                \
	} while (0)

static inline int kfs_run_all_tests(const struct kfs_test_case *cases, int count)
{
	(void)__KFS_EXPECT_SEGV_handler; // to eliminate unused function warnings
	int executed = 0;
	for (int i = 0; i < count; ++i)
	{
		__builtin_printf("[RUN ] %s\n", cases[i].name);
		cases[i].fn();
		if (kfs_test_failures == 0)
		{
			__builtin_printf("[PASS] %s\n", cases[i].name);
		}
		else
		{
			/* failure already reported */
		}
		executed++;
	}
	if (kfs_test_failures)
	{
		__builtin_printf("== SUMMARY: %d tests, %d failed ==\n", executed, kfs_test_failures);
	}
	else
	{
		__builtin_printf("== SUMMARY: %d tests, all passed ==\n", executed);
	}
	return kfs_test_failures == 0 ? 0 : 1;
}

#endif // KFS_HOST_TEST_FRAMEWORK_H
