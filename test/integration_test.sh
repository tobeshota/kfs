#!/usr/bin/env bash
set -euo pipefail

# Integration test runner
# - Builds the kernel via top-level Makefile (Dockerized toolchain)
# - Discovers and runs test cases under test/init/integration/*.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd -P)"
ARTIFACTS_DIR="$SCRIPT_DIR/_artifacts/integration"
mkdir -p "$ARTIFACTS_DIR"

echo "[integration] Build kernel (via Docker image if needed)"
make -C "$REPO_ROOT" -s kernel

# Common runtime configuration
TIMEOUT_BIN="${TIMEOUT_BIN:-timeout}"
TIMEOUT_SECS="${TIMEOUT_SECS:-20}"
ISA="${ISA:-i386}"

# Resolve timeout command (support macOS gtimeout)
if ! command -v "$TIMEOUT_BIN" >/dev/null 2>&1; then
	if command -v gtimeout >/dev/null 2>&1; then
		TIMEOUT_BIN=gtimeout
	else
		echo "[integration] ERROR: timeout (or gtimeout) not found. Please install coreutils." >&2
		exit 2
	fi
fi

# Function: run kernel and capture serial output to provided log file path
run_kernel_capture() {
	local log_file="$1" tmp_log="$log_file.tmp"
	local qemu_bin="qemu-system-${ISA}"
	local qemu_args="-kernel $REPO_ROOT/kfs.bin -serial file:$tmp_log -display none -no-reboot -no-shutdown"

	# Ensure kernel binary exists (should already after build)
	if [[ ! -f "$REPO_ROOT/kfs.bin" ]]; then
		echo "[integration] Rebuilding kernel (missing kfs.bin)" >&2
		make -C "$REPO_ROOT" -s kernel || return 1
	fi

	if command -v "$qemu_bin" >/dev/null 2>&1; then
		"$TIMEOUT_BIN" "${TIMEOUT_SECS}s" $qemu_bin $qemu_args >/dev/null 2>&1 || true
	else
		local docker_bin="${DOCKER:-docker}"
		local image="${IMAGE:-smizuoch/kfs:1.0.1}"
		if command -v "$docker_bin" >/dev/null 2>&1; then
			"$TIMEOUT_BIN" "${TIMEOUT_SECS}s" \
				"$docker_bin" run --rm -v "$REPO_ROOT":/work -w /work "$image" \
				qemu-system-"$ISA" $qemu_args >/dev/null 2>&1 || true
		else
			# Fallback: make target (may use docker/toolchain inside)
			"$TIMEOUT_BIN" "${TIMEOUT_SECS}s" make -s -C "$REPO_ROOT" run-kernel >/dev/null 2>&1 || true
			# Attempt to relocate output if run-kernel used stdio redirection; not guaranteed.
		fi
	fi

	tr -d '\r' <"$tmp_log" >"$log_file" 2>/dev/null || true
	rm -f "$tmp_log"
}

echo "[integration] Running tests in $SCRIPT_DIR/init/integration"

shopt -s nullglob
tests=("$SCRIPT_DIR"/init/integration/test_*.sh)
shopt -u nullglob

if ((${#tests[@]} == 0)); then
	echo "No integration tests found under $SCRIPT_DIR/init/integration"
	exit 1
fi

passed=0
failed=0

for t in "${tests[@]}"; do
	name="$(basename "$t")"
	echo "[integration] >>> $name"
	log_file="$ARTIFACTS_DIR/${name%.sh}.log"
	run_kernel_capture "$log_file"
	if LOG_FILE="$log_file" bash -eu "$t"; then
		echo "[integration] PASS: $name"
		passed=$((passed + 1))
	else
		echo "[integration] FAIL: $name (see $log_file)"
		failed=$((failed + 1))
	fi
done

echo "[integration] Summary: ${passed} passed, ${failed} failed"

if ((failed > 0)); then
	exit 1
fi

exit 0
