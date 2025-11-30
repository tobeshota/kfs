#!/bin/bash
# diff形式のカバレッジレポートを生成する
# インストルメント済みファイルをコピーし、COVERAGE_LINE()をマークする

COVERAGE_DIR="build/coverage"
REPORT_DIR="coverage/report"
SERIAL_LOG="coverage/log/qemu_serial.log"
MANIFEST_FILE="coverage/log/coverage_manifest.txt"

# 必要なファイルが存在するか確認
if [ ! -f "$SERIAL_LOG" ]; then
	echo "Error: $SERIAL_LOG not found. Run 'make unit' first."
	exit 1
fi

if [ ! -f "$MANIFEST_FILE" ]; then
	echo "Error: $MANIFEST_FILE not found. Run 'make coverage-instrument' first."
	exit 1
fi

if [ ! -d "$COVERAGE_DIR" ]; then
	echo "Error: $COVERAGE_DIR not found. Run 'make coverage-instrument' first."
	exit 1
fi

# レポートディレクトリをクリーンアップして作成
rm -rf "$REPORT_DIR"
mkdir -p "$REPORT_DIR"

# QEMUログから実行された行を抽出
# 形式: ファイル名:行番号:実行フラグ (1=実行済み, 0=未実行)
awk '/COVERAGE_START/,/COVERAGE_END/' "$SERIAL_LOG" |
	grep -v 'COVERAGE_START\|COVERAGE_END' |
	tr -d '\r' |
	awk -F: '$3 == "1" {print $1":"$2}' |
	sort -u >/tmp/executed_lines.txt

# ファイル書き込み完了を待機
sync

echo "Generating diff-style coverage report..."

# マニフェスト内の各ファイルを処理
current_file=""
# 標準入力から1行読み，それを区切り文字":"の前後で変数fileと変数lineに格納する
while IFS=: read -r file line; do
	[ -z "$file" ] || [ -z "$line" ] && continue

	# 新しいファイルを検出
	if [ "$file" != "$current_file" ]; then
		# 前のファイルが存在すれば保存
		if [ -n "$current_file" ] && [ -f "/tmp/current_report.c" ]; then
			# 相対パスを抽出（build/coverage/プレフィックスを除去）
			rel_path="${current_file#build/coverage/}"
			output_file="$REPORT_DIR/${rel_path}.diff"

			# ディレクトリ構造を作成
			mkdir -p "$(dirname "$output_file")"

			# 一時ファイルを最終場所に移動
			mv /tmp/current_report.c "$output_file"
			echo "  ✓ $rel_path.diff"
		fi

		# 新しいファイルの処理を開始
		current_file="$file"

		# インストルメント済みファイルを一時ファイルにコピー
		if [ -f "$file" ]; then
			cp "$file" /tmp/current_report.c
		else
			echo "  ✗ Warning: $file not found"
			continue
		fi
	fi

	# この行が実行されたかチェック
	if grep -q "^${file}:${line}$" /tmp/executed_lines.txt; then
		# 実行済み: ${line}行目のCOVERAGE_LINE()を +	COVERAGE_LINE() に置換
		sed -i '' "${line}s/^[[:space:]]*COVERAGE_LINE();$/+	COVERAGE_LINE();/" /tmp/current_report.c
	else
		# 未実行: ${line}行目のCOVERAGE_LINE()を -	COVERAGE_LINE() に置換
		sed -i '' "${line}s/^[[:space:]]*COVERAGE_LINE();$/-	COVERAGE_LINE();/" /tmp/current_report.c
	fi

done <"$MANIFEST_FILE"

# 最後のファイルを保存
if [ -n "$current_file" ] && [ -f "/tmp/current_report.c" ]; then
	rel_path="${current_file#build/coverage/}"
	output_file="$REPORT_DIR/${rel_path}.diff"
	mkdir -p "$(dirname "$output_file")"
	mv /tmp/current_report.c "$output_file"
	echo "  ✓ $rel_path.diff"
fi

# 一時ファイルをクリーンアップ
rm -f /tmp/executed_lines.txt /tmp/current_report.c

echo ""
echo "Diff-style coverage report generated in: $REPORT_DIR"
