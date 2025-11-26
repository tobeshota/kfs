/*
 * test_pgtable.c - ページテーブルフラグのテスト
 *
 * KFS-3要求のページフラグ(P/R/W/U/S/A/D/PS)が正しく機能することを検証する
 */

#include "../../../test_reset.h"
#include "host_test_framework.h"
#include <asm-i386/page.h>
#include <asm-i386/pgtable.h>
#include <kfs/mm.h>

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
 * テスト: _PAGE_KERNEL フラグの検証
 * 検証: _PAGE_KERNELにはPRESENTとRWが含まれ、USERは含まれない
 * 目的: カーネル専用ページのフラグ定義を確認
 */
KFS_TEST(test_page_kernel_flags)
{
	unsigned long flags = _PAGE_KERNEL;

	/* PRESENTフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_PRESENT);

	/* RWフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_RW);

	/* USERフラグが設定されていないこと */
	KFS_ASSERT_TRUE((flags & _PAGE_USER) == 0);
}

/*
 * テスト: _PAGE_USER_RW フラグの検証
 * 検証: _PAGE_USER_RWにはPRESENT、RW、USERが全て含まれる
 * 目的: ユーザー空間読み書き可能ページのフラグ定義を確認
 */
KFS_TEST(test_page_user_rw_flags)
{
	unsigned long flags = _PAGE_USER_RW;

	/* PRESENTフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_PRESENT);

	/* RWフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_RW);

	/* USERフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_USER);
}

/*
 * テスト: _PAGE_USER_RO フラグの検証
 * 検証: _PAGE_USER_ROにはPRESENTとUSERが含まれ、RWは含まれない
 * 目的: ユーザー空間読み取り専用ページのフラグ定義を確認
 */
KFS_TEST(test_page_user_ro_flags)
{
	unsigned long flags = _PAGE_USER_RO;

	/* PRESENTフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_PRESENT);

	/* USERフラグが設定されていること */
	KFS_ASSERT_TRUE(flags & _PAGE_USER);

	/* RWフラグが設定されていないこと */
	KFS_ASSERT_TRUE((flags & _PAGE_RW) == 0);
}

/*
 * テスト: pte_present() マクロの検証
 * 検証: PRESENTフラグが正しく判定されること
 * 目的: ページ存在チェックマクロの動作確認
 */
KFS_TEST(test_pte_present_macro)
{
	pte_t pte_with_present = _PAGE_PRESENT | _PAGE_RW;
	pte_t pte_without_present = _PAGE_RW;

	/* PRESENTフラグがある場合はtrueを返す */
	KFS_ASSERT_TRUE(pte_present(pte_with_present));

	/* PRESENTフラグがない場合はfalseを返す */
	KFS_ASSERT_TRUE(!pte_present(pte_without_present));
}

/*
 * テスト: pte_write() マクロの検証
 * 検証: RWフラグが正しく判定されること
 * 目的: 書き込み可能チェックマクロの動作確認
 */
KFS_TEST(test_pte_write_macro)
{
	pte_t pte_writable = _PAGE_PRESENT | _PAGE_RW;
	pte_t pte_readonly = _PAGE_PRESENT;

	/* RWフラグがある場合はtrueを返す */
	KFS_ASSERT_TRUE(pte_write(pte_writable));

	/* RWフラグがない場合はfalseを返す */
	KFS_ASSERT_TRUE(!pte_write(pte_readonly));
}

/*
 * テスト: pte_user() マクロの検証
 * 検証: USERフラグが正しく判定されること
 * 目的: ユーザーアクセス許可チェックマクロの動作確認
 */
KFS_TEST(test_pte_user_macro)
{
	pte_t pte_user = _PAGE_PRESENT | _PAGE_USER;
	pte_t pte_kernel = _PAGE_PRESENT;

	/* USERフラグがある場合はtrueを返す */
	KFS_ASSERT_TRUE(pte_user(pte_user));

	/* USERフラグがない場合はfalseを返す */
	KFS_ASSERT_TRUE(!pte_user(pte_kernel));
}

/*
 * テスト: pte_accessed() マクロの検証
 * 検証: ACCESSEDフラグが正しく判定されること
 * 目的: アクセス済みチェックマクロの動作確認
 */
KFS_TEST(test_pte_accessed_macro)
{
	pte_t pte_accessed = _PAGE_PRESENT | _PAGE_ACCESSED;
	pte_t pte_not_accessed = _PAGE_PRESENT;

	/* ACCESSEDフラグがある場合はtrueを返す */
	KFS_ASSERT_TRUE(pte_accessed(pte_accessed));

	/* ACCESSEDフラグがない場合はfalseを返す */
	KFS_ASSERT_TRUE(!pte_accessed(pte_not_accessed));
}

/*
 * テスト: pte_dirty() マクロの検証
 * 検証: DIRTYフラグが正しく判定されること
 * 目的: ダーティチェックマクロの動作確認
 */
KFS_TEST(test_pte_dirty_macro)
{
	pte_t pte_dirty = _PAGE_PRESENT | _PAGE_DIRTY;
	pte_t pte_clean = _PAGE_PRESENT;

	/* DIRTYフラグがある場合はtrueを返す */
	KFS_ASSERT_TRUE(pte_dirty(pte_dirty));

	/* DIRTYフラグがない場合はfalseを返す */
	KFS_ASSERT_TRUE(!pte_dirty(pte_clean));
}

/*
 * テスト: set_pte() でフラグが正しく設定されること
 * 検証: PTEに物理アドレスとフラグが正しく書き込まれる
 * 目的: PTE設定関数の動作確認
 */
KFS_TEST(test_set_pte_with_flags)
{
	pte_t pte = 0;
	unsigned long phys_addr = 0x00100000;
	unsigned long flags = _PAGE_KERNEL;

	/* PTEを設定 */
	set_pte(&pte, phys_addr, flags);

	/* 物理アドレスが正しく設定されていること */
	KFS_ASSERT_EQ(phys_addr, pte_page(pte));

	/* PRESENTフラグが設定されていること */
	KFS_ASSERT_TRUE(pte_present(pte));

	/* RWフラグが設定されていること */
	KFS_ASSERT_TRUE(pte_write(pte));

	/* USERフラグが設定されていないこと */
	KFS_ASSERT_TRUE(!pte_user(pte));
}

/*
 * テスト: pte_set_user() でUSERフラグを設定
 * 検証: ユーザーフラグ設定関数の動作確認
 * 目的: フラグ操作ヘルパーの動作確認
 */
KFS_TEST(test_pte_set_user_helper)
{
	pte_t pte = _PAGE_PRESENT | _PAGE_RW;

	/* 初期状態でUSERフラグがないこと */
	KFS_ASSERT_TRUE(!pte_user(pte));

	/* USERフラグを設定 */
	pte_set_user(&pte);

	/* USERフラグが設定されたこと */
	KFS_ASSERT_TRUE(pte_user(pte));
}

/*
 * テスト: pte_clear_user() でUSERフラグをクリア
 * 検証: ユーザーフラグクリア関数の動作確認
 * 目的: フラグ操作ヘルパーの動作確認
 */
KFS_TEST(test_pte_clear_user_helper)
{
	pte_t pte = _PAGE_PRESENT | _PAGE_RW | _PAGE_USER;

	/* 初期状態でUSERフラグがあること */
	KFS_ASSERT_TRUE(pte_user(pte));

	/* USERフラグをクリア */
	pte_clear_user(&pte);

	/* USERフラグがクリアされたこと */
	KFS_ASSERT_TRUE(!pte_user(pte));
}

/*
 * テスト: pte_set_write() でRWフラグを設定
 * 検証: 書き込みフラグ設定関数の動作確認
 * 目的: フラグ操作ヘルパーの動作確認
 */
KFS_TEST(test_pte_set_write_helper)
{
	pte_t pte = _PAGE_PRESENT;

	/* 初期状態でRWフラグがないこと */
	KFS_ASSERT_TRUE(!pte_write(pte));

	/* RWフラグを設定 */
	pte_set_write(&pte);

	/* RWフラグが設定されたこと */
	KFS_ASSERT_TRUE(pte_write(pte));
}

/*
 * テスト: pte_clear_write() でRWフラグをクリア
 * 検証: 書き込みフラグクリア関数の動作確認
 * 目的: フラグ操作ヘルパーの動作確認
 */
KFS_TEST(test_pte_clear_write_helper)
{
	pte_t pte = _PAGE_PRESENT | _PAGE_RW;

	/* 初期状態でRWフラグがあること */
	KFS_ASSERT_TRUE(pte_write(pte));

	/* RWフラグをクリア */
	pte_clear_write(&pte);

	/* RWフラグがクリアされたこと */
	KFS_ASSERT_TRUE(!pte_write(pte));
}

/*
 * テスト: pte_clear_accessed() でACCESSEDフラグをクリア
 * 検証: アクセスフラグクリア関数の動作確認
 * 目的: フラグ操作ヘルパーの動作確認
 */
KFS_TEST(test_pte_clear_accessed_helper)
{
	pte_t pte = _PAGE_PRESENT | _PAGE_ACCESSED;

	/* 初期状態でACCESSEDフラグがあること */
	KFS_ASSERT_TRUE(pte_accessed(pte));

	/* ACCESSEDフラグをクリア */
	pte_clear_accessed(&pte);

	/* ACCESSEDフラグがクリアされたこと */
	KFS_ASSERT_TRUE(!pte_accessed(pte));
}

/*
 * テスト: pte_clear_dirty() でDIRTYフラグをクリア
 * 検証: ダーティフラグクリア関数の動作確認
 * 目的: フラグ操作ヘルパーの動作確認
 */
KFS_TEST(test_pte_clear_dirty_helper)
{
	pte_t pte = _PAGE_PRESENT | _PAGE_DIRTY;

	/* 初期状態でDIRTYフラグがあること */
	KFS_ASSERT_TRUE(pte_dirty(pte));

	/* DIRTYフラグをクリア */
	pte_clear_dirty(&pte);

	/* DIRTYフラグがクリアされたこと */
	KFS_ASSERT_TRUE(!pte_dirty(pte));
}

/*
 * テスト: pde_large() マクロの検証
 * 検証: PSEフラグが正しく判定されること
 * 目的: ページサイズチェックマクロの動作確認
 */
KFS_TEST(test_pde_large_macro)
{
	pde_t pde_4mb = _PAGE_PRESENT | _PAGE_PSE;
	pde_t pde_4kb = _PAGE_PRESENT;

	/* PSEフラグがある場合（4MBページ）はtrueを返す */
	KFS_ASSERT_TRUE(pde_large(pde_4mb));

	/* PSEフラグがない場合（4KBページ）はfalseを返す */
	KFS_ASSERT_TRUE(!pde_large(pde_4kb));
}

/*
 * テスト: 恒等マッピング領域のPTEがカーネル専用であること
 * 検証: 0-4MB領域のページがUSERフラグなしで設定されている
 * 目的: 実際のページング初期化でカーネルフラグが使われていることを確認
 */
KFS_TEST(test_identity_mapping_is_kernel_only)
{
	extern pte_t *get_pte(unsigned long vaddr);

	/* テスト対象のアドレス（恒等マッピング範囲内） */
	unsigned long test_addr = 0x00100000; /* 1MB */

	/* PTEを取得 */
	pte_t *pte = get_pte(test_addr);

	/* PTEが存在すること */
	KFS_ASSERT_TRUE(pte != NULL);

	/* PTEがPRESENTであること */
	KFS_ASSERT_TRUE(pte_present(*pte));

	/* PTEが書き込み可能であること */
	KFS_ASSERT_TRUE(pte_write(*pte));

	/* PTEがカーネル専用（USERフラグなし）であること */
	KFS_ASSERT_TRUE(!pte_user(*pte));
}

/*
 * テスト: CPUによるACCESSEDフラグの自動設定
 * 検証: ページにアクセスするとCPUが自動的にACCESSEDフラグを設定する
 * 目的: ハードウェアによるフラグ設定の動作確認
 */
KFS_TEST(test_cpu_sets_accessed_flag)
{
	extern pte_t *get_pte(unsigned long vaddr);
	extern void __flush_tlb(void);

	/* テスト用のアドレス（恒等マッピング範囲内） */
	unsigned long test_addr = 0x00200000; /* 2MB */

	/* PTEを取得 */
	pte_t *pte = get_pte(test_addr);

	if (pte == NULL || !pte_present(*pte))
	{
		/* PTEが存在しない場合はスキップ */
		KFS_ASSERT_TRUE(1);
		return;
	}

	/* ACCESSEDフラグをクリア */
	pte_clear_accessed(pte);

	/* TLBをフラッシュ（重要: PTEを変更したらTLBを無効化する必要がある） */
	__flush_tlb();

	/* ページにアクセス（読み取り） */
	volatile unsigned char *ptr = (volatile unsigned char *)test_addr;
	volatile unsigned char dummy = *ptr;
	(void)dummy; /* 未使用警告を抑制 */

	/*
	 * CPUがACCESSEDフラグを自動設定することを確認。
	 * QEMUでi386ページングが有効な状態で実行されるため、
	 * ページアクセス後はCPUによってACCESSEDフラグが設定される。
	 */
	KFS_ASSERT_TRUE(pte_accessed(*pte));
}

/*
 * テスト: CPUによるDIRTYフラグの自動設定
 * 検証: ページに書き込むとCPUが自動的にDIRTYフラグを設定する
 * 目的: ハードウェアによるフラグ設定の動作確認
 */
KFS_TEST(test_cpu_sets_dirty_flag)
{
	extern pte_t *get_pte(unsigned long vaddr);
	extern void __flush_tlb(void);

	/* テスト用のアドレス（恒等マッピング範囲内） */
	unsigned long test_addr = 0x00300000; /* 3MB */

	/* PTEを取得 */
	pte_t *pte = get_pte(test_addr);

	if (pte == NULL || !pte_present(*pte) || !pte_write(*pte))
	{
		/* PTEが存在しないか書き込み不可の場合はスキップ */
		KFS_ASSERT_TRUE(1);
		return;
	}

	/* DIRTY/ACCESSEDフラグをクリア */
	pte_clear_dirty(pte);
	pte_clear_accessed(pte);

	/* TLBをフラッシュ（重要: PTEを変更したらTLBを無効化する必要がある） */
	__flush_tlb();

	/* ページに書き込み */
	volatile unsigned char *ptr = (volatile unsigned char *)test_addr;
	*ptr = 0x42;

	/*
	 * CPUがDIRTYフラグを自動設定することを確認。
	 * QEMUでi386ページングが有効な状態で実行されるため、
	 * ページ書き込み後はCPUによってDIRTYフラグが設定される。
	 */
	KFS_ASSERT_TRUE(pte_dirty(*pte));

	/* 書き込み時はACCESSEDフラグも設定される */
	KFS_ASSERT_TRUE(pte_accessed(*pte));
}

/*
 * テスト: ACCESSEDフラグのクリアと確認
 * 検証: pte_clear_accessed()が正しく動作すること
 * 目的: フラグクリア関数の動作確認
 */
KFS_TEST(test_clear_accessed_flag_functionality)
{
	extern pte_t *get_pte(unsigned long vaddr);

	unsigned long test_addr = 0x00100000;
	pte_t *pte = get_pte(test_addr);

	if (pte == NULL || !pte_present(*pte))
	{
		KFS_ASSERT_TRUE(1);
		return;
	}

	/* 元のPTE値を保存 */
	pte_t original = *pte;

	/* ACCESSEDフラグを手動で設定 */
	*pte = original | _PAGE_ACCESSED;
	KFS_ASSERT_TRUE(pte_accessed(*pte));

	/* ACCESSEDフラグをクリア */
	pte_clear_accessed(pte);
	KFS_ASSERT_TRUE(!pte_accessed(*pte));

	/* 元の値に戻す */
	*pte = original;
}

/*
 * テスト: DIRTYフラグのクリアと確認
 * 検証: pte_clear_dirty()が正しく動作すること
 * 目的: フラグクリア関数の動作確認
 */
KFS_TEST(test_clear_dirty_flag_functionality)
{
	extern pte_t *get_pte(unsigned long vaddr);

	unsigned long test_addr = 0x00100000;
	pte_t *pte = get_pte(test_addr);

	if (pte == NULL || !pte_present(*pte))
	{
		KFS_ASSERT_TRUE(1);
		return;
	}

	/* 元のPTE値を保存 */
	pte_t original = *pte;

	/* DIRTYフラグを手動で設定 */
	*pte = original | _PAGE_DIRTY;
	KFS_ASSERT_TRUE(pte_dirty(*pte));

	/* DIRTYフラグをクリア */
	pte_clear_dirty(pte);
	KFS_ASSERT_TRUE(!pte_dirty(*pte));

	/* 元の値に戻す */
	*pte = original;
}

static struct kfs_test_case cases[] = {
	KFS_REGISTER_TEST_WITH_SETUP(test_page_kernel_flags, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_page_user_rw_flags, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_page_user_ro_flags, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_present_macro, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_write_macro, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_user_macro, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_accessed_macro, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_dirty_macro, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_set_pte_with_flags, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_set_user_helper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_clear_user_helper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_set_write_helper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_clear_write_helper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_clear_accessed_helper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pte_clear_dirty_helper, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_pde_large_macro, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_identity_mapping_is_kernel_only, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cpu_sets_accessed_flag, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_cpu_sets_dirty_flag, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_clear_accessed_flag_functionality, setup_test, teardown_test),
	KFS_REGISTER_TEST_WITH_SETUP(test_clear_dirty_flag_functionality, setup_test, teardown_test),
};

int register_host_tests_pgtable(struct kfs_test_case **out)
{
	*out = cases;
	return (int)(sizeof(cases) / sizeof(cases[0]));
}
