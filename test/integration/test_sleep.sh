#!/usr/bin/env bash
set -Eeuo pipefail

# sleep コマンドの統合テスト
# input: jiffies → sleep 1 → jiffies の順に実行し，
# 2 つの jiffies 値の差が 1000 tick 以上であることを確認する（HZ=1000）．

if [[ -z "${LOG_FILE:-}" ]]; then
	echo "ERROR: LOG_FILE not set (must be provided by integration_test.sh)" >&2
	exit 2
fi

# ログから jiffies=NNN 形式の行を 2 行取り出す
mapfile -t JIFFIES_VALUES < <(grep -oE 'jiffies=[0-9]+' "$LOG_FILE" | grep -oE '[0-9]+' | head -2)

if [[ "${#JIFFIES_VALUES[@]}" -lt 2 ]]; then
	echo "FAIL: could not find two jiffies values in log" >&2
	cat "$LOG_FILE" >&2
	exit 1
fi

BEFORE=${JIFFIES_VALUES[0]}
AFTER=${JIFFIES_VALUES[1]}
ELAPSED=$((AFTER - BEFORE))

if [[ "$ELAPSED" -lt 1000 ]]; then
	echo "FAIL: elapsed=$ELAPSED ticks < 1000 — sleep returned too early" >&2
	cat "$LOG_FILE" >&2
	exit 1
fi

echo "OK: before=$BEFORE after=$AFTER elapsed=$ELAPSED ticks >= 1000 (sleep waited 1 second as expected)"
exit 0
