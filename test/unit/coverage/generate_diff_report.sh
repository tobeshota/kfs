#!/bin/bash
# Generate diff-style coverage report by copying instrumented files and marking COVERAGE_LINE()

COVERAGE_DIR="build/coverage"
REPORT_DIR="coverage/report"
SERIAL_LOG="build/log/qemu_serial.log"
MANIFEST_FILE="build/log/coverage_manifest.txt"

# Check if required files exist
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

# Clean and create report directory
rm -rf "$REPORT_DIR"
mkdir -p "$REPORT_DIR"

# Extract executed lines from QEMU log
awk '/COVERAGE_START/,/COVERAGE_END/' "$SERIAL_LOG" |
	grep -v 'COVERAGE_START\|COVERAGE_END' |
	tr -d '\r' |
	awk -F: '{print $1":"$2}' |
	sort -u >/tmp/executed_lines.txt

echo "Generating diff-style coverage report..."

# Process each file in manifest
current_file=""
while IFS=: read -r file line; do
	[ -z "$file" ] || [ -z "$line" ] && continue

	# New file encountered
	if [ "$file" != "$current_file" ]; then
		# Save previous file if it exists
		if [ -n "$current_file" ] && [ -f "/tmp/current_report.c" ]; then
			# Extract relative path (remove build/coverage/ prefix)
			rel_path="${current_file#build/coverage/}"
			output_file="$REPORT_DIR/${rel_path}.diff"

			# Create directory structure
			mkdir -p "$(dirname "$output_file")"

			# Move temporary file to final location
			mv /tmp/current_report.c "$output_file"
			echo "  ✓ $rel_path.diff"
		fi

		# Start processing new file
		current_file="$file"

		# Copy original instrumented file to temp
		if [ -f "$file" ]; then
			cp "$file" /tmp/current_report.c
		else
			echo "  ✗ Warning: $file not found"
			continue
		fi
	fi

	# Check if this line was executed
	if grep -q "^${file}:${line}$" /tmp/executed_lines.txt; then
		# Mark as executed: replace COVERAGE_LINE() with +	COVERAGE_LINE()
		sed -i '' "${line}s/^[[:space:]]*COVERAGE_LINE();$/+	COVERAGE_LINE();/" /tmp/current_report.c
	else
		# Mark as not executed: replace COVERAGE_LINE() with -	COVERAGE_LINE()
		sed -i '' "${line}s/^[[:space:]]*COVERAGE_LINE();$/-	COVERAGE_LINE();/" /tmp/current_report.c
	fi

done <"$MANIFEST_FILE"

# Save the last file
if [ -n "$current_file" ] && [ -f "/tmp/current_report.c" ]; then
	rel_path="${current_file#build/coverage/}"
	output_file="$REPORT_DIR/${rel_path}.diff"
	mkdir -p "$(dirname "$output_file")"
	mv /tmp/current_report.c "$output_file"
	echo "  ✓ $rel_path.diff"
fi

# Cleanup
rm -f /tmp/executed_lines.txt /tmp/current_report.c

echo ""
echo "Diff-style coverage report generated in: $REPORT_DIR"
