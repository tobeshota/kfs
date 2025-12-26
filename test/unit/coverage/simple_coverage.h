#ifndef _SIMPLE_COVERAGE_H
#define _SIMPLE_COVERAGE_H

#include <kfs/stdint.h>

/* 追跡可能なソースコード行の最大数 */
#define MAX_COVERAGE_LINES 4096

/** カバレッジエントリ
 * @brief ソースコードの1行を表す
 */
struct coverage_entry
{
	const char *file;
	uint32_t line;
	uint8_t executed; /* 0 = 未実行, 1 = 実行済み */
};

/** 行が実行されたことを記録する
 * @param file ソースファイル名 (例: "mm/page_alloc.c")
 * @param line 行番号
 * @note この関数はCOVERAGE_LINE()マクロから呼び出される
 */
void coverage_record_line(const char *file, uint32_t line);

/** すべてのカバレッジデータをシリアルポートにダンプする
 * @brief
 * 「ファイル名:行番号:実行状態」のフォーマットで出力する
 * ここで1は実行済み，0は未実行である
 *   COVERAGE_START
 *   file1.c:10:1
 *   file1.c:11:1
 *   file1.c:15:0
 *   COVERAGE_END
 */
void coverage_dump(void);

/** 行の実行を記録する
 * @note このマクロは計装スクリプトによって自動的に挿入される
 * @see test/unit/coverage/insert_coverage_func.py
 */
#define COVERAGE_LINE() coverage_record_line(__FILE__, __LINE__)

#endif /* _SIMPLE_COVERAGE_H */
