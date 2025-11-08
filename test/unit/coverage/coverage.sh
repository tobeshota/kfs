#!/bin/bash
# Parse coverage output from QEMU serial and generate diff format

COVERAGE_FILE="build/log/coverage.diff"
SERIAL_LOG="build/log/qemu_serial.log"

# Check if serial log exists
if [ ! -f "$SERIAL_LOG" ]; then
	echo "Error: $SERIAL_LOG not found. Run 'make unit' first."
	exit 1
fi

# Extract coverage data between COVERAGE_START and COVERAGE_END
# Remove CR (\r) characters for cross-platform compatibility
awk '/COVERAGE_START/,/COVERAGE_END/' "$SERIAL_LOG" |
	grep -v 'COVERAGE_START\|COVERAGE_END' |
	tr -d '\r' >/tmp/coverage_raw.txt

# Parse and format as diff
{
	echo "=== KFS Coverage Report ==="
	echo ""

	# Group by file
	current_file=""
	while IFS=: read -r file line executed; do
		# Skip empty lines
		[ -z "$file" ] && continue

		if [ "$file" != "$current_file" ]; then
			[ -n "$current_file" ] && echo ""
			echo "--- $file ---"
			current_file="$file"
		fi

		if [ "$executed" = "1" ]; then
			echo "+ $line: EXECUTED"
		else
			echo "- $line: NOT EXECUTED"
		fi
	done </tmp/coverage_raw.txt

	echo ""
	echo "=== End of Coverage Report ==="
} >"$COVERAGE_FILE"

echo "Coverage report written to $COVERAGE_FILE"
rm -f /tmp/coverage_raw.txt
