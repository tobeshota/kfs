#!/bin/bash
# Parse coverage output from QEMU serial and generate diff format

COVERAGE_FILE="coverage/log/summary.diff"
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

# Count statistics
total_lines=$(wc -l <"$MANIFEST_FILE" | tr -d ' ')
executed_count=$(wc -l </tmp/executed_lines.txt | tr -d ' ')
not_executed=$((total_lines - executed_count))
coverage_percent=$((executed_count * 100 / total_lines))

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
} >"$COVERAGE_FILE"

echo "Coverage report written to $COVERAGE_FILE"
rm -f /tmp/coverage_raw.txt /tmp/executed_lines.txt
