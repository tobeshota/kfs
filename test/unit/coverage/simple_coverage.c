/* Simple line coverage tracker implementation */

#include "simple_coverage.h"
#include <kfs/printk.h>

/* Global coverage database */
static struct coverage_entry coverage_db[MAX_COVERAGE_LINES];
static uint32_t coverage_count = 0;

/**
 * find_or_create_entry - Find existing entry or create new one
 * @file: Source filename
 * @line: Line number
 * @return: Pointer to coverage entry, or NULL if database is full
 */
static struct coverage_entry *find_or_create_entry(const char *file, uint32_t line)
{
	uint32_t i;

	/* First, try to find existing entry */
	/* Use pointer comparison instead of strcmp to avoid early boot issues */
	for (i = 0; i < coverage_count; i++)
	{
		if (coverage_db[i].line == line && coverage_db[i].file == file)
		{
			return &coverage_db[i];
		}
	}

	/* Not found, create new entry */
	if (coverage_count >= MAX_COVERAGE_LINES)
	{
		return NULL; /* Database full */
	}

	coverage_db[coverage_count].file = file;
	coverage_db[coverage_count].line = line;
	coverage_db[coverage_count].executed = 0;
	coverage_count++;

	return &coverage_db[coverage_count - 1];
}

void coverage_record_line(const char *file, uint32_t line)
{
	struct coverage_entry *entry;

	entry = find_or_create_entry(file, line);
	if (entry)
	{
		entry->executed = 1;
	}
}

void coverage_dump(void)
{
	uint32_t i;

	printk("COVERAGE_START\n");

	for (i = 0; i < coverage_count; i++)
	{
		/* Output format: filename:line:executed */
		printk("%s:%u:%u\n", coverage_db[i].file, coverage_db[i].line, coverage_db[i].executed);
	}

	printk("COVERAGE_END\n");
}
