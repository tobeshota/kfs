#!/usr/bin/env bash
set -Eeuo pipefail

if [[ -z "${LOG_FILE:-}" ]]; then
	echo "ERROR: LOG_FILE not set (must be provided by integration_test.sh)" >&2
	exit 2
fi

if grep -m1 -x '42' "$LOG_FILE" >/dev/null 2>&1; then
	echo "Found expected output: 42"
	exit 0
else
	echo "Did not observe expected output '42'. See $LOG_FILE" >&2
	exit 1
fi
