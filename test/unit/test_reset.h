/**
 * テスト用: 共通リセット関数のヘッダー
 */

#ifndef KFS_TEST_RESET_H
#define KFS_TEST_RESET_H

/**
 * 全サブシステムを初期状態にリセット
 * @details
 * 全テストで統一的に使用するリセット関数。
 * テストの独立性を保証するために、各テスト前に呼び出す。
 */
void reset_all_state_for_test(void);

#endif /* KFS_TEST_RESET_H */
