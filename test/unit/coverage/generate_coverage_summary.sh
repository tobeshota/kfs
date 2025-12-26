#!/bin/bash
# QEMUシリアル出力からカバレッジデータを解析してdiff形式のレポートを生成

COVERAGE_FILE="coverage/log/summary.txt"
SERIAL_LOG="coverage/log/qemu_serial.log"
MANIFEST_FILE="coverage/log/coverage_manifest.txt"

# シリアルログの存在確認
if [ ! -f "$SERIAL_LOG" ]; then
	echo "Error: $SERIAL_LOG not found. Run 'make unit' first."
	exit 1
fi

# マニフェストの存在確認
if [ ! -f "$MANIFEST_FILE" ]; then
	echo "Error: $MANIFEST_FILE not found. Run 'make coverage-instrument' first."
	exit 1
fi

# COVERAGE_STARTとCOVERAGE_ENDの間のカバレッジデータを抽出
# クロスプラットフォーム互換性のためCR(\r)文字を削除
awk '/COVERAGE_START/,/COVERAGE_END/' "$SERIAL_LOG" |
	grep -v 'COVERAGE_START\|COVERAGE_END' |
	tr -d '\r' >/tmp/coverage_raw.txt

# 実行された行のソート済みリストを作成
sort -u /tmp/coverage_raw.txt | awk -F: '{print $1":"$2}' >/tmp/executed_lines.txt

# 全体統計のカウント
total_lines=$(wc -l <"$MANIFEST_FILE" | tr -d ' ')
executed_count=$(wc -l </tmp/executed_lines.txt | tr -d ' ')
not_executed=$((total_lines - executed_count))
coverage_percent=$((executed_count * 100 / total_lines))

# ファイル別統計の生成
# まず、ユニークなファイルを取得し、ファイルごとの総行数をカウント
awk -F: '{print $1}' "$MANIFEST_FILE" | sort | uniq >/tmp/unique_files.txt

# 各ファイルについて、総行数と実行済み行数をカウント
>/tmp/file_stats.txt
while read -r file; do
	[ -z "$file" ] && continue

	# このファイルの総行数をカウント
	total=$(grep -c "^${file}:" "$MANIFEST_FILE")

	# このファイルの実行済み行数をカウント
	executed=$(grep "^${file}:" "$MANIFEST_FILE" | while IFS=: read -r f line; do
		if grep -q "^${f}:${line}$" /tmp/executed_lines.txt; then
			echo "1"
		fi
	done | wc -l | tr -d ' ')

	# パーセンテージを計算
	if [ "$total" -gt 0 ]; then
		percent=$((executed * 100 / total))
	else
		percent=0
	fi

	# 表示用にbuild/coverage/プレフィックスを削除
	display_file=$(echo "$file" | sed 's|build/coverage/||')

	echo "$executed $total $percent $display_file"
done </tmp/unique_files.txt | sort -k4 >/tmp/file_stats.txt

# マニフェストと実行済み行を比較してレポートを生成
{
	echo "=== KFS Coverage Report ==="
	echo ""

	# ファイルごとにグループ化
	current_file=""
	while IFS=: read -r file line; do
		# 空行をスキップ
		[ -z "$file" ] && continue

		if [ "$file" != "$current_file" ]; then
			[ -n "$current_file" ] && echo ""
			echo "--- $file ---"
			current_file="$file"
		fi

		# この行が実行されたかをgrepで確認
		if grep -q "^${file}:${line}$" /tmp/executed_lines.txt; then
			echo "+ $line: EXECUTED"
		else
			echo "- $line: NOT EXECUTED"
		fi
	done <"$MANIFEST_FILE"

	echo ""
	echo "=== Coverage Summary ==="
	echo "Total coverage points: $total_lines"
	echo "Executed: $executed_count ($coverage_percent%)"
	echo "Not executed: $not_executed"
	echo ""
	echo "=== End of Coverage Report ==="
	echo ""
	echo "=== Summary ==="
	echo "Run	/	Sum		coverage"

	# 一時ファイルからファイル別統計を出力
	while read -r executed total percent display_file; do
		printf "%d\t/\t%d\t\t%d%%\t./%s\n" "$executed" "$total" "$percent" "$display_file"
	done </tmp/file_stats.txt

	echo "— Total —- -- -- -- --"
	printf "%d\t/\t%d\t\t%d%%\n" "$executed_count" "$total_lines" "$coverage_percent"
	echo ""
	echo "For more,"
	echo "- summary: test/unit/coverage/log/summary.txt"
	echo "- code:    test/unit/coverage/report/"
} >"$COVERAGE_FILE"

echo "Coverage report written to $COVERAGE_FILE"
rm -f /tmp/coverage_raw.txt /tmp/executed_lines.txt /tmp/unique_files.txt /tmp/file_stats.txt
