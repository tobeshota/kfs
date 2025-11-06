#include "host_test_framework.h"
#include "kfs/string.h"

KFS_TEST(test_strlen)
{
	KFS_ASSERT_EQ(6, strlen("kernel"));
	KFS_ASSERT_EQ(4, strlen("from"));
	KFS_ASSERT_EQ(7, strlen("scratch"));
}

KFS_TEST(test_strnlen_clamps)
{
	const char *text = "hello";
	KFS_ASSERT_EQ(5, (long long)strnlen(text, 16));
	KFS_ASSERT_EQ(3, (long long)strnlen(text, 3));
	KFS_ASSERT_EQ(0, (long long)strnlen(text, 0));
}

KFS_TEST(test_strncpy_pads_with_nuls)
{
	char buf[8];
	char *ret = strncpy(buf, "abc", sizeof(buf));
	KFS_ASSERT_TRUE(ret == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "abc") == 0);
	for (size_t i = 4; i < sizeof(buf); i++)
		KFS_ASSERT_EQ(0, (long long)buf[i]);
	char partial[4] = {0};
	strncpy(partial, "abcdef", 2);
	KFS_ASSERT_TRUE(partial[0] == 'a');
	KFS_ASSERT_TRUE(partial[1] == 'b');
	KFS_ASSERT_EQ(0, (long long)partial[2]);
}

KFS_TEST(test_strncpy_zero_count)
{
	char buf[] = "keep";
	char *ret = strncpy(buf, "data", 0);
	KFS_ASSERT_TRUE(ret == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "keep") == 0);
}

KFS_TEST(test_strncpy_empty_source)
{
	char buf[4] = {'A', 'B', 'C', '\0'};
	char *ret = strncpy(buf, "", sizeof(buf) - 1);
	KFS_ASSERT_TRUE(ret == buf);
	for (size_t i = 0; i < sizeof(buf) - 1; i++)
		KFS_ASSERT_EQ(0, (long long)buf[i]);
	KFS_ASSERT_EQ('\0', (long long)buf[3]);
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
	KFS_ASSERT_TRUE(strcat(buf, "bar") == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foobar") == 0);
	KFS_ASSERT_TRUE(strncat(buf, "bazqux", 3) == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foobarbaz") == 0);
	size_t zero = 0;
	KFS_ASSERT_TRUE(strncat(buf, "ignored", zero) == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foobarbaz") == 0);
	char limited[16] = "foo";
	KFS_ASSERT_TRUE(strncat(limited, "bar", 2) == limited);
	KFS_ASSERT_TRUE(strcmp(limited, "fooba") == 0);
	char full[16] = "foo";
	KFS_ASSERT_TRUE(strncat(full, "bar", 10) == full);
	KFS_ASSERT_TRUE(strcmp(full, "foobar") == 0);
}

KFS_TEST(test_strncat_empty_source)
{
	char buf[8] = "foo";
	const char empty_src[] = "";
	KFS_ASSERT_TRUE(strncat(buf, empty_src, sizeof(buf) - 4) == buf);
	KFS_ASSERT_TRUE(strcmp(buf, "foo") == 0);
}

KFS_TEST(test_strcmp_family)
{
	KFS_ASSERT_TRUE(strcmp("abc", "abc") == 0);
	KFS_ASSERT_TRUE(strcmp("abc", "abd") < 0);
	KFS_ASSERT_TRUE(strcmp("abd", "abc") > 0);
	KFS_ASSERT_TRUE(strcmp("", "foo") < 0);
	KFS_ASSERT_TRUE(strncmp("alphabet", "alpha", 5) == 0);
	KFS_ASSERT_TRUE(strncmp("alpha", "alphabet", 7) < 0);
	KFS_ASSERT_TRUE(strncmp("abc", "abd", 3) < 0);
	size_t zero = 0;
	KFS_ASSERT_TRUE(strncmp("abc", "xyz", zero) == 0);
}

KFS_TEST(test_strchr_strrchr_strstr)
{
	const char *sample = "abracadabra";
	KFS_ASSERT_TRUE(strchr(sample, 'r') == sample + 2);
	KFS_ASSERT_TRUE(strrchr(sample, 'r') == sample + 9);
	KFS_ASSERT_TRUE(strstr(sample, "cada") == sample + 4);
	char empty_buf[1] = {'\0'};
	const char *empty = empty_buf;
	KFS_ASSERT_TRUE(strstr(sample, empty) == sample);
	KFS_ASSERT_TRUE(strchr(sample, 'z') == 0);
	KFS_ASSERT_TRUE(strrchr(sample, 'z') == 0);
	KFS_ASSERT_TRUE(strstr(sample, "xyz") == 0);
	KFS_ASSERT_TRUE(strstr("abcdef", "abz") == 0);
	KFS_ASSERT_TRUE(strstr("ab", "abc") == 0);
}

KFS_TEST(test_memset_and_memcpy)
{
	unsigned char buf[6];
	memset(buf, 0xAB, sizeof(buf));
	for (size_t i = 0; i < sizeof(buf); i++)
		KFS_ASSERT_EQ(0xAB, (long long)buf[i]);
	unsigned char other[6] = {0};
	memcpy(other, buf, sizeof(buf));
	for (size_t i = 0; i < sizeof(buf); i++)
		KFS_ASSERT_EQ((long long)buf[i], (long long)other[i]);
}

KFS_TEST(test_memmove_handles_overlap)
{
	char text[16] = "abcdef";
	memmove(text + 2, text, 4);
	KFS_ASSERT_TRUE(strcmp(text, "ababcd") == 0);
	memmove(text, text + 2, 4);
	KFS_ASSERT_TRUE(strcmp(text, "abcdcd") == 0);
	memmove(text, text, 3);
	KFS_ASSERT_TRUE(strcmp(text, "abcdcd") == 0);
	memmove(text, text + 1, 0);
	KFS_ASSERT_TRUE(strcmp(text, "abcdcd") == 0);
	memmove(text, text + 2, 2);
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
	KFS_REGISTER_TEST(test_strlen),
	KFS_REGISTER_TEST(test_strnlen_clamps),
	KFS_REGISTER_TEST(test_strncpy_pads_with_nuls),
	KFS_REGISTER_TEST(test_strncpy_zero_count),
	KFS_REGISTER_TEST(test_strncpy_empty_source),
	KFS_REGISTER_TEST(test_strlcpy_behaviour),
	KFS_REGISTER_TEST(test_strcat_and_strncat),
	KFS_REGISTER_TEST(test_strncat_empty_source),
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
