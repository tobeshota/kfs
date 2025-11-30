#!/bin/bash
# Parse coverage output from QEMU serial and generate diff format

COVERAGE_FILE="coverage/log/summary.txt"
SERIAL_LOG="coverage/log/qemu_serial.log"
MANIFEST_FILE="coverage/log/coverage_manifest.txt"

# Check if serial log exists
if [ ! -f "$SERIAL_LOG" ]; then
	echo "Error: $SERIAL_LOG not found. Run 'make unit' first."
	exit 1
fi

# Check if manifest exists
if [ ! -f "$MANIFEST_FILE" ]; then
	echo "Error: $MANIFEST_FILE not found. Run 'make coverage-instrument' first."
	exit 1
fi

# Extract coverage data between COVERAGE_START and COVERAGE_END
# Remove CR (\r) characters for cross-platform compatibility
awk '/COVERAGE_START/,/COVERAGE_END/' "$SERIAL_LOG" |
	grep -v 'COVERAGE_START\|COVERAGE_END' |
	tr -d '\r' >/tmp/coverage_raw.txt

# Create a sorted list of executed lines
sort -u /tmp/coverage_raw.txt | awk -F: '{print $1":"$2}' >/tmp/executed_lines.txt

# Count overall statistics
total_lines=$(wc -l <"$MANIFEST_FILE" | tr -d ' ')
executed_count=$(wc -l </tmp/executed_lines.txt | tr -d ' ')
not_executed=$((total_lines - executed_count))
coverage_percent=$((executed_count * 100 / total_lines))

# Generate per-file statistics
# First, get unique files and count total lines per file
awk -F: '{print $1}' "$MANIFEST_FILE" | sort | uniq >/tmp/unique_files.txt

# For each file, count total and executed lines
>/tmp/file_stats.txt
while read -r file; do
	[ -z "$file" ] && continue

	# Count total lines for this file
	total=$(grep -c "^${file}:" "$MANIFEST_FILE")

	# Count executed lines for this file
	executed=$(grep "^${file}:" "$MANIFEST_FILE" | while IFS=: read -r f line; do
		if grep -q "^${f}:${line}$" /tmp/executed_lines.txt; then
			echo "1"
		fi
	done | wc -l | tr -d ' ')

	# Calculate percentage
	if [ "$total" -gt 0 ]; then
		percent=$((executed * 100 / total))
	else
		percent=0
	fi

	# Remove build/coverage/ prefix for display
	display_file=$(echo "$file" | sed 's|build/coverage/||')

	echo "$executed $total $percent $display_file"
done </tmp/unique_files.txt | sort -k4 >/tmp/file_stats.txt

# Generate report by comparing manifest with executed lines
{
	echo "=== KFS Coverage Report ==="
	echo ""

	# Group by file
	current_file=""
	while IFS=: read -r file line; do
		# Skip empty lines
		[ -z "$file" ] && continue

		if [ "$file" != "$current_file" ]; then
			[ -n "$current_file" ] && echo ""
			echo "--- $file ---"
			current_file="$file"
		fi

		# Check if this line was executed using grep
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

	# Print per-file statistics from temp file
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
