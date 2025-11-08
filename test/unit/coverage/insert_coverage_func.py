#!/usr/bin/env python3
"""
プロダクションコードのCファイルをカバレッジ計測用に一括で計装する

このスクリプトは:
1. プロダクションコードのCファイルを検索（test/ディレクトリは除外）
2. 各ファイルにCOVERAGE_LINE()マクロを挿入
3. 計装済みファイルをbuild/coverage/にディレクトリ構造を維持して出力

使い方:
    python3 insert_coverage_func.py <ソースルート> <出力ディレクトリ>

例:
    python3 insert_coverage_func.py ../../ build/coverage/
"""

import sys
import re
from pathlib import Path


def instrument_c_file(input_path, output_path):
    """
    Cソースファイルをカバレッジマクロで計装

    引数:
        input_path: 入力Cファイルのパス
        output_path: 計装済みCファイルの出力パス
    """
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # ENABLE_COVERAGEのifdefブロックを削除（常にカバレッジを有効化）
    content = remove_coverage_ifdefs(content)

    # simple_coverage.hのincludeを追加（まだない場合）
    if 'simple_coverage.h' not in content:
        # 最後の#include行を探す
        lines = content.split('\n')
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i

        if last_include_idx >= 0:
            # 最後のincludeの後に挿入
            lines.insert(last_include_idx + 1, '#include "coverage/simple_coverage.h"')
            content = '\n'.join(lines)
        else:
            # includeが見つからない場合は先頭に追加
            content = '#include "coverage/simple_coverage.h"\n' + content

    # 関数を計装
    instrumented = instrument_functions(content)

    # 必要に応じて出力ディレクトリを作成
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # 計装済みファイルを書き込み
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(instrumented)

    return True


def instrument_functions(content):
    """
    各関数の先頭にCOVERAGE_LINE()を挿入

    戦略（ブレース整形を前提）:
    1. 独立した'{'行（独自の行にある開き波括弧）を検索
    2. 前の行に関数シグネチャが含まれているかチェック
    3. 開き波括弧の後にCOVERAGE_LINE();を挿入
    """
    lines = content.split('\n')
    result = []
    i = 0

    while i < len(lines):
        line = lines[i]
        result.append(line)

        # この行が独立した開き波括弧かチェック
        stripped = line.strip()
        if stripped == '{':
            # 関数本体の開始かチェック
            if is_function_brace(i, lines):
                # 適切なインデントでCOVERAGE_LINE()を追加
                indent = get_indent(line)
                result.append(indent + '\tCOVERAGE_LINE();')

        i += 1

    return '\n'.join(result)


def is_function_brace(brace_idx, lines):
    """
    brace_idxの位置にある'{'が関数の開き波括弧かを判定

    戦略:
    - 前の空でない行を確認
    - ')'で終わり、制御構文キーワードで始まらなければ、関数の可能性が高い
    """
    if brace_idx == 0:
        return False

    # 空でない、コメントでない最後の行を後方検索
    for i in range(brace_idx - 1, max(0, brace_idx - 10), -1):
        prev_line = lines[i].strip()

        # 空行をスキップ
        if not prev_line:
            continue

        # コメントをスキップ
        if prev_line.startswith('//') or prev_line.startswith('/*') or prev_line.startswith('*'):
            continue

        # 空でない行を発見
        # 関数シグネチャのように見えるかチェック: ')'で終わる
        if prev_line.endswith(')'):
            # 制御構文でないことを確認
            # 最初の単語を分割して取得
            words = prev_line.split()
            if not words:
                return False

            first_word = words[0]
            control_keywords = ['if', 'else', 'for', 'while', 'do', 'switch']

            if first_word in control_keywords:
                return False

            # struct/union/enumをチェック
            if any(kw in prev_line for kw in ['struct', 'union', 'enum']):
                return False

            # おそらく関数！
            return True

        # ')'で終わらない空でない行を見つけた場合、関数ではない
        return False

    return False


def get_indent(line):
    """行のインデントを取得"""
    return line[:len(line) - len(line.lstrip())]


def remove_coverage_ifdefs(content):
    """
    #ifdef ENABLE_COVERAGEブロックを削除

    別ディレクトリで計装しているため、
    これらのファイルではカバレッジは常に有効
    """
    lines = content.split('\n')
    result = []
    skip_level = 0

    for line in lines:
        stripped = line.strip()

        # #ifdef ENABLE_COVERAGEをチェック
        if stripped.startswith('#ifdef') and 'ENABLE_COVERAGE' in stripped:
            skip_level += 1
            continue

        # #ifndef ENABLE_COVERAGEもスキップ
        if stripped.startswith('#ifndef') and 'ENABLE_COVERAGE' in stripped:
            skip_level += 1
            continue

        # 対応する#endifをチェック
        if stripped.startswith('#endif') and skip_level > 0:
            skip_level -= 1
            continue

        # スキップ中でなければ行を追加
        if skip_level == 0:
            result.append(line)

    return '\n'.join(result)


def find_production_c_files(root_dir):
    """
    プロダクションコードのCソースファイルを検索

    除外対象:
    - test/ディレクトリ配下のファイル
    - build/ディレクトリ配下のファイル

    引数:
        root_dir: 検索開始ディレクトリ

    戻り値:
        CファイルのPathオブジェクトのリスト
    """
    root = Path(root_dir)
    c_files = []

    for c_file in root.rglob('*.c'):
        # ルートからの相対パスを取得
        rel_path = c_file.relative_to(root)
        path_parts = rel_path.parts

        # テストファイルをスキップ
        if 'test' in path_parts:
            continue

        # buildディレクトリをスキップ
        if 'build' in path_parts:
            continue

        c_files.append(c_file)

    return c_files


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 insert_coverage_func.py <source_root> <output_directory>")
        print("Example: python3 insert_coverage_func.py ../../ build/coverage/")
        sys.exit(1)

    source_root = Path(sys.argv[1]).resolve()
    output_dir = Path(sys.argv[2]).resolve()

    if not source_root.exists():
        print(f"Error: Source root '{source_root}' does not exist")
        sys.exit(1)

    print(f"Searching for production C files in: {source_root}")
    c_files = find_production_c_files(source_root)

    if not c_files:
        print("No production C files found")
        sys.exit(1)

    print(f"Found {len(c_files)} production C files:")
    for f in c_files:
        print(f"  {f.relative_to(source_root)}")

    print(f"\nInstrumenting files (output to: {output_dir})")

    success_count = 0
    error_count = 0

    for c_file in c_files:
        # ディレクトリ構造を維持して出力パスを計算
        rel_path = c_file.relative_to(source_root)
        output_file = output_dir / rel_path

        try:
            instrument_c_file(c_file, output_file)
            print(f"  ✓ {rel_path}")
            success_count += 1
        except Exception as e:
            print(f"  ✗ {rel_path}: {e}")
            error_count += 1

    print(f"\nInstrumentation complete:")
    print(f"  Success: {success_count}")
    print(f"  Errors: {error_count}")

    if error_count > 0:
        sys.exit(1)


if __name__ == '__main__':
    main()
