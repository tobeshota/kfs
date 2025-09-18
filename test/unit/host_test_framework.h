#ifndef KFS_HOST_TEST_FRAMEWORK_H
#define KFS_HOST_TEST_FRAMEWORK_H

/* Use low-level write(2) instead of stdio buffering (fprintf). */
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef void (*kfs_test_fn)(void);

	struct kfs_test_case
	{
		const char *name;
		kfs_test_fn fn;
	};

	/* ---------------------------------------------------------
	 * Minimal formatting helpers (only what we need):
	 *   - decimal signed integers (int, long long)
	 *   - strings
	 * The goal is to avoid stdio (fprintf/snprintf) and just use write().
	 * --------------------------------------------------------- */
	static inline void kfs_write_all(int fd, const char *buf, size_t len)
	{
		/* Best-effort write loop (rare for such short buffers to partial write). */
		while (len > 0)
		{
			ssize_t w = write(fd, buf, len);
			if (w <= 0)
			{
				/* On error we just stop; test output loss is acceptable here. */
				return;
			}
			buf += (size_t)w;
			len -= (size_t)w;
		}
	}

	static inline void kfs_buf_append(char *buf, size_t *idx, size_t cap, const char *s)
	{
		while (*s && *idx + 1 < cap)
		{
			buf[(*idx)++] = *s++;
		}
		buf[*idx] = '\0';
	}

	static inline void kfs_buf_append_ll(char *buf, size_t *idx, size_t cap, long long v)
	{
		char tmp[32];
		int neg = 0;
		unsigned long long x;
		if (v < 0)
		{
			neg = 1;
			x = (unsigned long long)(-v);
		}
		else
		{
			x = (unsigned long long)v;
		}
		int pos = 0;
		if (x == 0)
		{
			tmp[pos++] = '0';
		}
		while (x > 0 && pos < (int)sizeof(tmp))
		{
			tmp[pos++] = (char)('0' + (x % 10));
			x /= 10;
		}
		if (neg && *idx + 1 < cap)
		{
			buf[(*idx)++] = '-';
		}
		while (pos-- > 0 && *idx + 1 < cap)
		{
			buf[(*idx)++] = tmp[pos];
		}
		buf[*idx] = '\0';
	}

	static inline void kfs_report_fail_eq(const char *file, int line, const char *exp_s, const char *act_s, long long e,
										  long long a)
	{
		char buf[512];
		size_t idx = 0;
		kfs_buf_append(buf, &idx, sizeof(buf), "[FAIL] ");
		kfs_buf_append(buf, &idx, sizeof(buf), file);
		kfs_buf_append(buf, &idx, sizeof(buf), ":");
		kfs_buf_append_ll(buf, &idx, sizeof(buf), line);
		kfs_buf_append(buf, &idx, sizeof(buf), " ");
		kfs_buf_append(buf, &idx, sizeof(buf), exp_s);
		kfs_buf_append(buf, &idx, sizeof(buf), " == ");
		kfs_buf_append(buf, &idx, sizeof(buf), act_s);
		kfs_buf_append(buf, &idx, sizeof(buf), "  expected=");
		kfs_buf_append_ll(buf, &idx, sizeof(buf), e);
		kfs_buf_append(buf, &idx, sizeof(buf), " actual=");
		kfs_buf_append_ll(buf, &idx, sizeof(buf), a);
		kfs_buf_append(buf, &idx, sizeof(buf), "\n");
		kfs_write_all(2, buf, idx);
	}

	static inline void kfs_report_fail_true(const char *file, int line, const char *expr)
	{
		char buf[256];
		size_t idx = 0;
		kfs_buf_append(buf, &idx, sizeof(buf), "[FAIL] ");
		kfs_buf_append(buf, &idx, sizeof(buf), file);
		kfs_buf_append(buf, &idx, sizeof(buf), ":");
		kfs_buf_append_ll(buf, &idx, sizeof(buf), line);
		kfs_buf_append(buf, &idx, sizeof(buf), " ");
		kfs_buf_append(buf, &idx, sizeof(buf), expr);
		kfs_buf_append(buf, &idx, sizeof(buf), " is false\n");
		kfs_write_all(2, buf, idx);
	}

	static inline void kfs_report_simple(int fd, const char *tag, const char *name)
	{
		char buf[256];
		size_t idx = 0;
		kfs_buf_append(buf, &idx, sizeof(buf), tag); /* already contains brackets + space */
		kfs_buf_append(buf, &idx, sizeof(buf), name);
		kfs_buf_append(buf, &idx, sizeof(buf), "\n");
		kfs_write_all(fd, buf, idx);
	}

	static inline void kfs_report_summary(int failed, int executed)
	{
		char buf[256];
		size_t idx = 0;
		if (failed)
		{
			kfs_buf_append(buf, &idx, sizeof(buf), "== SUMMARY: ");
			kfs_buf_append_ll(buf, &idx, sizeof(buf), executed);
			kfs_buf_append(buf, &idx, sizeof(buf), " tests, ");
			kfs_buf_append_ll(buf, &idx, sizeof(buf), failed);
			kfs_buf_append(buf, &idx, sizeof(buf), " failed ==\n");
			kfs_write_all(2, buf, idx);
		}
		else
		{
			kfs_buf_append(buf, &idx, sizeof(buf), "== SUMMARY: ");
			kfs_buf_append_ll(buf, &idx, sizeof(buf), executed);
			kfs_buf_append(buf, &idx, sizeof(buf), " tests, all passed ==\n");
			kfs_write_all(1, buf, idx);
		}
	}

#define KFS_TEST(name) static void name(void)

#define KFS_ASSERT_EQ(expected, actual)                                                                                \
	do                                                                                                                 \
	{                                                                                                                  \
		long long _e = (long long)(expected);                                                                          \
		long long _a = (long long)(actual);                                                                            \
		if (_e != _a)                                                                                                  \
		{                                                                                                              \
			kfs_report_fail_eq(__FILE__, __LINE__, #expected, #actual, _e, _a);                                        \
			kfs_test_failures++;                                                                                       \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

#define KFS_ASSERT_TRUE(expr)                                                                                          \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(expr))                                                                                                   \
		{                                                                                                              \
			kfs_report_fail_true(__FILE__, __LINE__, #expr);                                                           \
			kfs_test_failures++;                                                                                       \
			return;                                                                                                    \
		}                                                                                                              \
	} while (0)

	extern int kfs_test_failures;

#define KFS_REGISTER_TEST(fn)                                                                                          \
	{                                                                                                                  \
#fn, fn                                                                                                        \
	}

	static inline int kfs_run_all_tests(const struct kfs_test_case *cases, int count)
	{
		int executed = 0;
		for (int i = 0; i < count; ++i)
		{
			kfs_report_simple(1, "[RUN ] ", cases[i].name);
			cases[i].fn();
			if (kfs_test_failures == 0)
			{
				kfs_report_simple(1, "[PASS] ", cases[i].name);
			}
			else
			{
				/* failure already reported */
			}
			executed++;
		}
		kfs_report_summary(kfs_test_failures, executed);
		return kfs_test_failures == 0 ? 0 : 1;
	}

#ifdef __cplusplus
}
#endif

#endif // KFS_HOST_TEST_FRAMEWORK_H
