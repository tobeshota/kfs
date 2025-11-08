#!/usr/bin/env python3
"""
プロダクションコードのCファイルをカバレッジ計測用に一括で計装する

このスクリプトは:
1. プロダクションコードのCファイルを検索（test/ディレクトリは除外）
2. 各ファイルにCOVERAGE_LINE()マクロを挿入
3. 計装済みファイルをbuild/coverage/にディレクトリ構造を維持して出力
4. マニフェストファイルに全ての計装位置を記録

使い方:
    python3 insert_coverage_func.py <ソースルート> <出力ディレクトリ> <マニフェストファイル>

例:
    python3 insert_coverage_func.py ../../ build/coverage/ build/log/coverage_manifest.txt
"""

import sys
import re
from pathlib import Path


# グローバルなマニフェストリスト
coverage_manifest = []


def instrument_c_file(input_path, output_path, source_root, output_dir):
    """
    Cソースファイルをカバレッジマクロで計装

    引数:
        input_path: 入力Cファイルのパス
        output_path: 計装済みCファイルの出力パス
        source_root: ソースルートパス（相対パス計算用）
        output_dir: 出力ディレクトリ（マニフェスト用パス計算）
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

    # 相対パスを計算（output_dirからの相対パス + "build/coverage/"プレフィックス）
    rel_path = output_path.relative_to(output_dir)
    # マニフェスト用にbuild/coverage/プレフィックスを追加
    manifest_path = f"build/coverage/{rel_path}"

    # 関数を計装（マニフェストに記録しながら）
    instrumented = instrument_functions(content, manifest_path)

    # 必要に応じて出力ディレクトリを作成
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # 計装済みファイルを書き込み
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(instrumented)

    return True


def instrument_functions(content, file_path):
    """
    各関数の先頭にCOVERAGE_LINE()を挿入

    引数:
        content: ファイル内容
        file_path: ファイルパス（マニフェスト記録用）

    戦略（ブレース整形を前提）:
    1. 独立した'{'行（独自の行にある開き波括弧）を検索
    2. 前の行に関数シグネチャが含まれているかチェック
    3. 開き波括弧の後にCOVERAGE_LINE();を挿入
    4. 挿入した行番号をマニフェストに記録
    """
    lines = content.split('\n')
    result = []
    i = 0
    line_offset = 0  # 挿入による行番号のオフセット

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

                # マニフェストに記録（出力ファイルでの行番号）
                inserted_line = len(result)
                coverage_manifest.append(f"{file_path}:{inserted_line}")

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
    if len(sys.argv) != 4:
        print("Usage: python3 insert_coverage_func.py <source_root> <output_directory> <manifest_file>")
        print("Example: python3 insert_coverage_func.py ../../ build/coverage/ build/log/coverage_manifest.txt")
        sys.exit(1)

    source_root = Path(sys.argv[1]).resolve()
    output_dir = Path(sys.argv[2]).resolve()
    manifest_file = Path(sys.argv[3])

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
            instrument_c_file(c_file, output_file, source_root, output_dir)
            print(f"  ✓ {rel_path}")
            success_count += 1
        except Exception as e:
            print(f"  ✗ {rel_path}: {e}")
            error_count += 1

    # マニフェストファイルを書き込み
    manifest_file.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_file, 'w', encoding='utf-8') as f:
        for entry in coverage_manifest:
            f.write(entry + '\n')

    print(f"\nInstrumentation complete:")
    print(f"  Success: {success_count}")
    print(f"  Errors: {error_count}")
    print(f"  Coverage points: {len(coverage_manifest)}")
    print(f"  Manifest written to: {manifest_file}")

    if error_count > 0:
        sys.exit(1)


if __name__ == '__main__':
    main()
