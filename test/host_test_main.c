#include <stdio.h>

#include "host_test_framework.h"

int kfs_test_failures = 0;

// ここに外部テスト宣言 (各テストファイルで KFS_TEST 定義)

int main(void) {
  extern int register_host_tests(struct kfs_test_case * *out);
  struct kfs_test_case* cases = NULL;
  int count = register_host_tests(&cases);
  return kfs_run_all_tests(cases, count);
}
