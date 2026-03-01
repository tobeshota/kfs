/*
 * mm/mmap.c の以下の関数をテスト:
 * - do_mmap(): 匿名メモリマッピングの確保
 * - do_munmap(): メモリマッピングの解放
 */

#include "../test_reset.h"
#include "unit_test_framework.h"
#include <kfs/mm.h>
#include <kfs/mman.h>
#include <kfs/stddef.h>

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

/*
 * テスト: do_mmap - MAP_ANONYMOUS で有効なアドレスが返る
 * 検証: NULL でないアドレスが返ること
 */
KFS_TEST(test_do_mmap_returns_valid_address)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE(addr != NULL);
	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);
}

/*
 * テスト: do_mmap - mmap 後に find_vma() が VMA を返す
 * 検証: insert_vm_area() が正しく動作していること
 */
KFS_TEST(test_do_mmap_inserts_vma)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE(addr != NULL);
	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);

	struct vm_area_struct *vma = find_vma((unsigned long)addr);
	KFS_ASSERT_TRUE(vma != NULL);
	KFS_ASSERT_EQ(vma->vm_start, (unsigned long)addr);
}

/*
 * テスト: do_mmap - 確保した領域へ読み書きできる
 * 検証: ページがマッピングされ、実際に書き込み可能であること
 */
KFS_TEST(test_do_mmap_write_readable)
{
	unsigned char *buf = (unsigned char *)do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)buf != (unsigned long)MAP_FAILED);

	/* 書き込みと読み返しが一致することを確認 */
	buf[0] = 0xAB;
	buf[4095] = 0xCD;
	KFS_ASSERT_EQ((unsigned char)0xAB, (unsigned char)buf[0]);
	KFS_ASSERT_EQ((unsigned char)0xCD, (unsigned char)buf[4095]);
}

/*
 * テスト: do_munmap - munmap 後に find_vma() が NULL を返す
 * 検証: VMA がリストから削除されること
 */
KFS_TEST(test_do_munmap_removes_vma)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)addr != (unsigned long)MAP_FAILED);

	int ret = do_munmap((unsigned long)addr, 4096);
	KFS_ASSERT_EQ(0, ret);

	/* VMA がリストから消えていること */
	struct vm_area_struct *vma = find_vma((unsigned long)addr);
	KFS_ASSERT_TRUE(vma == NULL);
}

/*
 * テスト: do_mmap - len = 0 で MAP_FAILED を返す
 */
KFS_TEST(test_do_mmap_zero_len_fails)
{
	void *addr = do_mmap(NULL, 0, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)addr == (unsigned long)MAP_FAILED);
}

/*
 * テスト: do_mmap - MAP_ANONYMOUS なしで MAP_FAILED を返す
 * 検証: ファイルマッピングは現実装では未サポート
 */
KFS_TEST(test_do_mmap_no_anonymous_fails)
{
	void *addr = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)addr == (unsigned long)MAP_FAILED);
}

/*
 * テスト: do_mmap - 複数回確保したアドレスが重複しない
 */
KFS_TEST(test_do_mmap_multiple_non_overlapping)
{
	void *a1 = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	void *a2 = do_mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE);
	KFS_ASSERT_TRUE((unsigned long)a1 != (unsigned long)MAP_FAILED);
	KFS_ASSERT_TRUE((unsigned long)a2 != (unsigned long)MAP_FAILED);
	/* 二つのアドレスは異なること */
	KFS_ASSERT_TRUE((unsigned long)a1 != (unsigned long)a2);
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_returns_valid_address, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_inserts_vma, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_write_readable, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_munmap_removes_vma, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_zero_len_fails, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_no_anonymous_fails, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_do_mmap_multiple_non_overlapping, setup_test, teardown_test),
};

int register_unit_tests_mmap(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
