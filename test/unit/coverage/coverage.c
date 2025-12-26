/* シンプルな行カバレッジトラッカーの実装 */

#include "coverage.h"
#include <kfs/printk.h>

/* グローバルカバレッジデータベース */
static struct coverage_entry coverage_db[MAX_COVERAGE_LINES];
static uint32_t coverage_count = 0;

/** 既存エントリを検索、または新規作成する
 * @param file ソースファイル名
 * @param line 行番号
 * @return カバレッジエントリへのポインタ．
 *         データベースが満杯の場合NULL
 */
static struct coverage_entry *find_or_create_entry(const char *file, uint32_t line)
{
	uint32_t i;

	/* 既存エントリを検索する */
	for (i = 0; i < coverage_count; i++)
	{
		if (coverage_db[i].line == line && coverage_db[i].file == file)
		{
			return &coverage_db[i];
		}
	}

	/* 見つからない場合，新規エントリを作成する */
	if (coverage_count >= MAX_COVERAGE_LINES)
	{
		return NULL; /* データベースが満杯 */
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
		/* 出力フォーマット: ファイル名:行番号:実行状態 */
		printk("%s:%u:%u\n", coverage_db[i].file, coverage_db[i].line, coverage_db[i].executed);
	}

	printk("COVERAGE_END\n");
}
