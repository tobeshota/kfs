#include "host_test_framework.h"
#include <linux/string.h>

static char *(*volatile kfs_strcat_fn)(char *, const char *) = strcat;
static char *(*volatile kfs_strncat_fn)(char *, const char *, size_t) = strncat;
static char *(*volatile kfs_strncpy_fn)(char *, const char *, size_t) = strncpy;
static void *(*volatile kfs_memcpy_fn)(void *, const void *, size_t) = memcpy;
static void *(*volatile kfs_memmove_fn)(void *, const void *, size_t) = memmove;
static void *(*volatile kfs_memset_fn)(void *, int, size_t) = memset;
static int (*volatile kfs_strncmp_fn)(const char *, const char *, size_t) = strncmp;
static char *(*volatile kfs_strstr_fn)(const char *, const char *) = strstr;

KFS_TEST(test_strnlen_clamps)
{
	const char *text = "hello";
	KFS_ASSERT_EQ(5, (long long)strnlen(text, 16));
	KFS_ASSERT_EQ(3, (long long)strnlen(text, 3));
}

KFS_TEST(test_strncpy_pads_with_nuls)
{
	char buf[8];
	char *ret = kfs_strncpy_fn(buf, "abc", sizeof(buf));
	KFS_ASSERT_TRUE(ret == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "abc") == 0);
	for (size_t i = 4; i < sizeof(buf); i++)
		KFS_ASSERT_EQ(0, (long long)buf[i]);
	char partial[4] = {0};
	kfs_strncpy_fn(partial, "abcdef", 2);
	KFS_ASSERT_TRUE(partial[0] == 'a');
	KFS_ASSERT_TRUE(partial[1] == 'b');
	KFS_ASSERT_EQ(0, (long long)partial[2]);
}

KFS_TEST(test_strncpy_zero_count)
{
	char buf[] = "keep";
	char *ret = kfs_strncpy_fn(buf, "data", 0);
	KFS_ASSERT_TRUE(ret == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "keep") == 0);
}

KFS_TEST(test_strlcpy_behaviour)
{
	char buf[5];
	size_t copied = strlcpy(buf, "world", sizeof(buf));
	KFS_ASSERT_EQ(5, (long long)copied);
	KFS_ASSERT_TRUE(strcmp(buf, "worl") == 0);

	char guard[3] = {'X', 'Y', '\0'};
	size_t only_len = strlcpy(guard, "hi", 0);
	KFS_ASSERT_EQ(2, (long long)only_len);
	KFS_ASSERT_EQ('X', (long long)guard[0]);

	char roomy[8];
	size_t len2 = strlcpy(roomy, "hi", sizeof(roomy));
	KFS_ASSERT_EQ(2, (long long)len2);
	KFS_ASSERT_TRUE(strcmp(roomy, "hi") == 0);
}

KFS_TEST(test_strcat_and_strncat)
{
	char buf[16] = "foo";
	KFS_ASSERT_TRUE(kfs_strcat_fn(buf, "bar") == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foobar") == 0);
	KFS_ASSERT_TRUE(kfs_strncat_fn(buf, "bazqux", 3) == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foobarbaz") == 0);
	KFS_ASSERT_TRUE(kfs_strncat_fn(buf, "ignored", 0) == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foobarbaz") == 0);
	char limited[16] = "foo";
	KFS_ASSERT_TRUE(kfs_strncat_fn(limited, "bar", 2) == limited);
	KFS_ASSERT_TRUE(strcmp(limited, "fooba") == 0);
	char full[16] = "foo";
	KFS_ASSERT_TRUE(kfs_strncat_fn(full, "bar", 10) == full);
	KFS_ASSERT_TRUE(strcmp(full, "foobar") == 0);
}

KFS_TEST(test_strcmp_family)
{
	KFS_ASSERT_TRUE(strcmp("abc", "abc") == 0);
	KFS_ASSERT_TRUE(strcmp("abc", "abd") < 0);
	KFS_ASSERT_TRUE(strcmp("abd", "abc") > 0);
	KFS_ASSERT_TRUE(strcmp("", "foo") < 0);
	KFS_ASSERT_TRUE(kfs_strncmp_fn("alphabet", "alpha", 5) == 0);
	KFS_ASSERT_TRUE(kfs_strncmp_fn("alpha", "alphabet", 7) < 0);
	KFS_ASSERT_TRUE(kfs_strncmp_fn("abc", "abd", 3) < 0);
	KFS_ASSERT_TRUE(kfs_strncmp_fn("abc", "xyz", 0) == 0);
}

KFS_TEST(test_strchr_strrchr_strstr)
{
	const char *sample = "abracadabra";
	KFS_ASSERT_TRUE(strchr(sample, 'r') == sample + 2);
	KFS_ASSERT_TRUE(strrchr(sample, 'r') == sample + 9);
	KFS_ASSERT_TRUE(kfs_strstr_fn(sample, "cada") == sample + 4);
	KFS_ASSERT_TRUE(kfs_strstr_fn(sample, "") == sample);
	KFS_ASSERT_TRUE(strchr(sample, 'z') == 0);
	KFS_ASSERT_TRUE(kfs_strstr_fn(sample, "xyz") == 0);
	KFS_ASSERT_TRUE(kfs_strstr_fn("abcdef", "abz") == 0);
	KFS_ASSERT_TRUE(kfs_strstr_fn("ab", "abc") == 0);
}

KFS_TEST(test_memset_and_memcpy)
{
	unsigned char buf[6];
	kfs_memset_fn(buf, 0xAB, sizeof(buf));
	for (size_t i = 0; i < sizeof(buf); i++)
		KFS_ASSERT_EQ(0xAB, (long long)buf[i]);
	unsigned char other[6] = {0};
	kfs_memcpy_fn(other, buf, sizeof(buf));
	for (size_t i = 0; i < sizeof(buf); i++)
		KFS_ASSERT_EQ((long long)buf[i], (long long)other[i]);
}

KFS_TEST(test_memmove_handles_overlap)
{
	char text[16] = "abcdef";
	kfs_memmove_fn(text + 2, text, 4);
	KFS_ASSERT_TRUE(strcmp(text, "ababcd") == 0);
	kfs_memmove_fn(text, text + 2, 4);
	KFS_ASSERT_TRUE(strcmp(text, "abcdcd") == 0);
	kfs_memmove_fn(text, text, 3);
	KFS_ASSERT_TRUE(strcmp(text, "abcdcd") == 0);
	kfs_memmove_fn(text, text + 1, 0);
	KFS_ASSERT_TRUE(strcmp(text, "abcdcd") == 0);
	kfs_memmove_fn(text, text + 2, 2);
	KFS_ASSERT_TRUE(strncmp(text, "cd", 2) == 0);
}

KFS_TEST(test_memcmp_and_memchr)
{
	unsigned char lhs[] = {1, 2, 3, 4};
	unsigned char rhs[] = {1, 2, 3, 5};
	KFS_ASSERT_TRUE(memcmp(lhs, rhs, 3) == 0);
	KFS_ASSERT_TRUE(memcmp(lhs, rhs, 4) < 0);
	unsigned char data[] = {0, 1, 2, 3, 4};
	void *found = memchr(data, 2, sizeof(data));
	KFS_ASSERT_TRUE(found == &data[2]);
	KFS_ASSERT_TRUE(memchr(data, 9, sizeof(data)) == 0);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST(test_strnlen_clamps),
	KFS_REGISTER_TEST(test_strncpy_pads_with_nuls),
	KFS_REGISTER_TEST(test_strncpy_zero_count),
	KFS_REGISTER_TEST(test_strlcpy_behaviour),
	KFS_REGISTER_TEST(test_strcat_and_strncat),
	KFS_REGISTER_TEST(test_strcmp_family),
	KFS_REGISTER_TEST(test_strchr_strrchr_strstr),
	KFS_REGISTER_TEST(test_memset_and_memcpy),
	KFS_REGISTER_TEST(test_memmove_handles_overlap),
	KFS_REGISTER_TEST(test_memcmp_and_memchr),
};

int register_host_tests_string(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
