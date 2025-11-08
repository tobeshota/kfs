/* Simple line coverage tracker for freestanding environment */

#ifndef _SIMPLE_COVERAGE_H
#define _SIMPLE_COVERAGE_H

#include <kfs/stdint.h>

/* Maximum number of unique source lines we can track */
#define MAX_COVERAGE_LINES 4096

/* Coverage entry: represents one line of source code */
struct coverage_entry
{
	const char *file;
	uint32_t line;
	uint8_t executed; /* 0 = not executed, 1 = executed */
};

/**
 * coverage_record_line - Record that a line was executed
 * @file: Source filename (e.g., "mm/page_alloc.c")
 * @line: Line number
 *
 * This function is called by COVERAGE_LINE() macro.
 * It's designed to be as lightweight as possible.
 */
void coverage_record_line(const char *file, uint32_t line);

/**
 * coverage_dump - Dump all coverage data to serial port
 *
 * Output format:
 *   COVERAGE_START
 *   file1.c:10:1
 *   file1.c:11:1
 *   file1.c:15:0
 *   COVERAGE_END
 *
 * Where the format is: filename:line:executed
 * executed: 1 = executed, 0 = not executed
 */
void coverage_dump(void);

/**
 * COVERAGE_LINE - Macro to record line execution
 *
 * Usage:
 *   COVERAGE_LINE();
 *
 * This will be automatically inserted by our instrumentation script.
 * Always enabled for instrumented files.
 */
#define COVERAGE_LINE() coverage_record_line(__FILE__, __LINE__)

#endif /* _SIMPLE_COVERAGE_H */
