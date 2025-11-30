#!/usr/bin/env bash
set -Eeuo pipefail

# panic コマンドによるカーネルパニックをテスト
# 期待される出力:
# 1. "Kernel panic: Test panic from shell command" というメッセージ
# 2. スタックトレース表示
# 3. "---[ end Kernel panic ]---" で終了

if [[ -z "${LOG_FILE:-}" ]]; then
	echo "ERROR: LOG_FILE not set (must be provided by integration_test.sh)" >&2
	exit 2
fi

echo "Checking for kernel panic output in $LOG_FILE"

# パニックメッセージの確認
if ! grep -q "Kernel panic: Test panic from shell command" "$LOG_FILE"; then
	echo "ERROR: Did not find expected panic message" >&2
	echo "--- Log contents ---" >&2
	cat "$LOG_FILE" >&2
	exit 1
fi

# スタックトレースセクションの確認
if ! grep -q "\-\-\-\[ stack trace \]\-\-\-" "$LOG_FILE"; then
	echo "ERROR: Did not find stack trace section" >&2
	exit 1
fi

# パニック終了メッセージの確認
if ! grep -q "\-\-\-\[ end Kernel panic \]\-\-\-" "$LOG_FILE"; then
	echo "ERROR: Did not find panic end message" >&2
	exit 1
fi

echo "✓ Found expected panic message"
echo "✓ Found stack trace section"
echo "✓ Found panic end message"
echo "Kernel panic test PASSED"

exit 0
