#!/usr/bin/env bash
set -Eeuo pipefail

# sched コマンドの統合テスト
# cmd_sched() が ring-3 プロセスを 50 個 fork/exec_fn/wait ループし、
# '-' と '_' を交互に出力して改行で終わることを確認する。
#
# 期待出力（50文字）: -_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-

if [[ -z "${LOG_FILE:-}" ]]; then
	echo "ERROR: LOG_FILE not set (must be provided by integration_test.sh)" >&2
	exit 2
fi

# 期待出力と完全一致する行が存在すること
EXPECTED="-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_"

if grep -Fxq -- "$EXPECTED" "$LOG_FILE"; then
	echo "Found expected sched output: $EXPECTED"
	exit 0
else
	echo "Did not observe expected sched output '$EXPECTED'. See $LOG_FILE" >&2
	cat "$LOG_FILE" >&2
	exit 1
fi
