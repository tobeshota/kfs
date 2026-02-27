#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd -P)"
ARTIFACTS_DIR="$SCRIPT_DIR/_artifacts"
mkdir -p "$ARTIFACTS_DIR"

# Load environment variables from .env
if [[ -f "$REPO_ROOT/.env" ]]; then
	set -a # 自動エクスポートを有効化
	source "$REPO_ROOT/.env"
	set +a # 自動エクスポートを無効化
fi

echo "[integration] Build kernel (via Docker image if needed)"
make -C "$REPO_ROOT" -s kernel

TIMEOUT_BIN="${TIMEOUT_BIN:-timeout}"
TIMEOUT_SECS="${TIMEOUT_SECS:-8}"
ISA="${ISA:-i386}"

if ! command -v "$TIMEOUT_BIN" >/dev/null 2>&1; then
	if command -v gtimeout >/dev/null 2>&1; then
		TIMEOUT_BIN=gtimeout
	else
		echo "[integration] ERROR: timeout (or gtimeout) not found." >&2
		exit 2
	fi
fi

run_kernel_capture() {
	local log_file="$1" tmp_log="$log_file.tmp"
	local input_file="${2:-}" # オプション: 入力ファイル
	local qemu_bin="qemu-system-${ISA}"

	# 1. 事前に kernel があるか確認
	if [[ ! -f "$REPO_ROOT/Image" ]]; then
		make -C "$REPO_ROOT" -s kernel || return 1
	fi

	# 入力ファイルがある場合、QEMUモニターのsendkeyコマンドに変換
	# これにより実際のキーボード割り込み（IRQ1）が発生する
	local monitor_script=""
	if [[ -n "$input_file" && -f "$input_file" ]]; then
		monitor_script=$(mktemp)
		# カーネル起動待ち（シェルプロンプト表示まで）
		echo "{ \"execute\": \"qmp_capabilities\" }" >"$monitor_script"
		# 入力ファイルの各文字をsendkeyコマンドに変換
		while IFS= read -r -n1 char || [[ -n "$char" ]]; do
			local key=""
			case "$char" in
			[a-z]) key="$char" ;;
			[A-Z]) key="shift-$(echo "$char" | tr '[:upper:]' '[:lower:]')" ;;
			[0-9]) key="$char" ;;
			' ') key="spc" ;;
			$'\n' | '') key="ret" ;;
			'-') key="minus" ;;
			'_') key="shift-minus" ;;
			'.') key="dot" ;;
			'/') key="slash" ;;
			*) continue ;;
			esac
			if [[ -n "$key" ]]; then
				echo "{ \"execute\": \"send-key\", \"arguments\": { \"keys\": [{\"type\": \"qcode\", \"data\": \"$key\"}] } }" >>"$monitor_script"
			fi
		done <"$input_file"
	fi

	# 2. ホストで直接 QEMU が動く場合
	if command -v "$qemu_bin" >/dev/null 2>&1; then
		if [[ -n "$monitor_script" && -f "$monitor_script" ]]; then
			# QMPモニターを使用してキーボード入力をシミュレート
			local qemu_args="-kernel $REPO_ROOT/Image -serial file:$tmp_log -display none -no-reboot -no-shutdown -qmp stdio"
			(
				sleep 0.5 # カーネル起動待ち
				cat "$monitor_script"
				sleep 2 # コマンド実行待ち
			) | "$TIMEOUT_BIN" "${TIMEOUT_SECS}s" $qemu_bin $qemu_args >/dev/null 2>&1 || true
		else
			# 入力ファイルがない場合は -serial file を使用
			local qemu_args="-kernel $REPO_ROOT/Image -serial file:$tmp_log -display none -no-reboot -no-shutdown"
			"$TIMEOUT_BIN" "${TIMEOUT_SECS}s" $qemu_bin $qemu_args >/dev/null 2>&1 || true
		fi
	else
		# 3. Docker 経由: /work に REPO_ROOT をマウントしているため、
		#    ホスト絶対パス -> /work への変換が必要。
		local docker_bin="${DOCKER:-docker}"
		local image="${IMAGE:-kfs-${ISA}-toolchain}"
		if command -v "$docker_bin" >/dev/null 2>&1; then
			local container_root="/work"
			# 例: /home/runner/work/kfs/kfs/test/integration/_artifacts/foo.log.tmp
			#  -> /work/test/integration/_artifacts/foo.log.tmp
			local container_tmp_log="${tmp_log/#$REPO_ROOT/$container_root}"

			if [[ -n "$monitor_script" && -f "$monitor_script" ]]; then
				# QMPモニターを使用してキーボード入力をシミュレート
				local container_monitor="${monitor_script/#$REPO_ROOT/$container_root}"
				# モニタースクリプトをコンテナ内にコピーするため、tmpファイルの位置をartifacts内に
				# log_file からテスト名を導出して並列実行時のファイル名衝突を回避する
				local _test_basename
				_test_basename="$(basename "${log_file%.log}")"
				local container_monitor_script="$ARTIFACTS_DIR/monitor_script_${_test_basename}.tmp"
				cp "$monitor_script" "$container_monitor_script"
				local container_monitor_path="${container_monitor_script/#$REPO_ROOT/$container_root}"
				local qemu_args="-kernel $container_root/Image -serial file:$container_tmp_log -display none -no-reboot -no-shutdown -qmp stdio"
				"$docker_bin" run --rm -v "$REPO_ROOT":$container_root -w $container_root "$image" \
					bash -c "(sleep 0.5; cat $container_monitor_path; sleep 2) | timeout ${TIMEOUT_SECS}s qemu-system-$ISA $qemu_args" >/dev/null 2>&1 || true
				rm -f "$container_monitor_script"
			else
				# 入力ファイルがない場合は -serial file を使用
				local qemu_args="-kernel $container_root/Image -serial file:$container_tmp_log -display none -no-reboot -no-shutdown"
				"$TIMEOUT_BIN" "${TIMEOUT_SECS}s" \
					"$docker_bin" run --rm -v "$REPO_ROOT":$container_root -w $container_root "$image" \
					qemu-system-"$ISA" $qemu_args >/dev/null 2>&1 || true
			fi
		else
			# 4. それでも QEMU を直接起動できない場合は既存の make run-kernel にフォールバック
			"$TIMEOUT_BIN" "${TIMEOUT_SECS}s" make -s -C "$REPO_ROOT" run-kernel >/dev/null 2>&1 || true
		fi
	fi

	# クリーンアップ
	[[ -n "$monitor_script" && -f "$monitor_script" ]] && rm -f "$monitor_script"

	# Docker 内で /work/... に生成されたファイルはマウントによりホスト側 $tmp_log と同一。
	tr -d '\r' <"$tmp_log" >"$log_file" 2>/dev/null || true
	rm -f "$tmp_log"
}

echo "[integration] Running tests in $SCRIPT_DIR"

shopt -s nullglob
tests=("$SCRIPT_DIR"/test_*.sh)
shopt -u nullglob

if ((${#tests[@]} == 0)); then
	echo "No integration tests found under $SCRIPT_DIR"
	exit 1
fi

# ── 並列実行 ─────────────────────────────────────────────────
# 各テストを独立したサブシェルでバックグラウンド起動し、
# 終了後に result ファイルで PASS/FAIL を集計する。
declare -A _pids

for t in "${tests[@]}"; do
	name="$(basename "$t" .sh)"
	log_file="$ARTIFACTS_DIR/${name}.log"
	result_file="$ARTIFACTS_DIR/${name}.result"
	out_file="$ARTIFACTS_DIR/${name}.out"
	input_file="${t%.sh}.input"

	echo "[integration] START: ${name}.sh"
	(
		if [[ -f "$input_file" ]]; then
			run_kernel_capture "$log_file" "$input_file" || true
		else
			run_kernel_capture "$log_file" || true
		fi
		if LOG_FILE="$log_file" bash -eu "$t"; then
			echo "PASS" >"$result_file"
		else
			echo "FAIL" >"$result_file"
		fi
	) >"$out_file" 2>&1 &
	_pids["$name"]=$!
done

# 全テスト完了を待機
for name in "${!_pids[@]}"; do
	wait "${_pids[$name]}" || true
done

# 結果集計（出力をテスト名順に表示）
passed=0
failed=0

for t in "${tests[@]}"; do
	name="$(basename "$t" .sh)"
	log_file="$ARTIFACTS_DIR/${name}.log"
	result_file="$ARTIFACTS_DIR/${name}.result"
	out_file="$ARTIFACTS_DIR/${name}.out"

	# サブシェルの出力を表示
	[[ -f "$out_file" ]] && cat "$out_file" && rm -f "$out_file"

	if [[ -f "$result_file" ]] && [[ "$(cat "$result_file")" == "PASS" ]]; then
		echo "[integration] PASS: ${name}.sh"
		passed=$((passed + 1))
	else
		echo "[integration] FAIL: ${name}.sh (see $log_file)"
		failed=$((failed + 1))
	fi
	rm -f "$result_file"
done

echo "[integration] Summary: ${passed} passed, ${failed} failed"

if ((failed > 0)); then
	exit 1
fi

exit 0
