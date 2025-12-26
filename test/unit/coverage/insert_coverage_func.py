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

    # coverage.hのincludeを追加（まだない場合）
    if 'coverage.h' not in content:
        # 最後の#include行を探す
        lines = content.split('\n')
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i

        if last_include_idx >= 0:
            # 最後のincludeの後に挿入
            lines.insert(last_include_idx + 1, '#include "coverage/coverage.h"')
            content = '\n'.join(lines)
        else:
            # includeが見つからない場合は先頭に追加
            content = '#include "coverage/coverage.h"\n' + content

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
    すべての実行可能文にCOVERAGE_LINE()を挿入（C1カバレッジ）

    引数:
        content: ファイル内容
        file_path: ファイルパス（マニフェスト記録用）

    戦略:
    1. 各文（セミコロンで終わる行）の前にCOVERAGE_LINE()を挿入
    2. 制御構文（if/while/for/switch）の前にCOVERAGE_LINE()を挿入
    3. return文の前にCOVERAGE_LINE()を挿入
    4. ただし、宣言のみやコメント行は除外
    5. 配列/構造体初期化子の中では挿入しない
    """
    lines = content.split('\n')
    result = []
    in_function = False
    brace_depth = 0
    initializer_depth = 0  # 配列/構造体初期化子の深さ
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # 空行やコメント行はそのまま追加
        if not stripped or stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
            result.append(line)
            i += 1
            continue

        # プリプロセッサディレクティブはそのまま追加
        if stripped.startswith('#'):
            result.append(line)
            i += 1
            continue

        # 配列/構造体初期化子の検出
        # "変数名[] = {" や "変数名 = {" のパターンを検出
        if '[] = {' in line or (re.search(r'\w+\s*=\s*\{', line) and not stripped.startswith('if') and not stripped.startswith('while') and not stripped.startswith('for')):
            # 初期化子の開始
            initializer_depth += line.count('{') - line.count('}')
            result.append(line)
            i += 1
            continue

        # 初期化子の中にいる場合
        if initializer_depth > 0:
            initializer_depth += line.count('{') - line.count('}')
            result.append(line)
            i += 1
            continue

        # ブレース深度を追跡
        if '{' in line:
            if stripped == '{' and is_function_brace(i, lines):
                in_function = True
                brace_depth = 1
                result.append(line)
                i += 1
                continue
            brace_depth += line.count('{')

        if '}' in line:
            brace_depth -= line.count('}')
            if brace_depth <= 0:
                in_function = False

        # 関数内でのみ計装
        if in_function and brace_depth > 0:
            should_instrument = False

            # else/else ifの前には挿入しない（if-elseチェーンを壊さないため）
            if stripped.startswith('else'):
                result.append(line)
                i += 1
                continue

            # 実行可能文かチェック
            # 1. セミコロンで終わる（文の終わり）
            if stripped.endswith(';'):
                # 継続行かチェック（インデントが深すぎる、または前の行が代入や関数呼び出しの途中）
                # 簡易判定: 行がタブ2つ以上のインデントなら継続行の可能性
                indent_level = len(line) - len(line.lstrip('\t'))
                # インデントが深い場合は継続行の可能性があるため、前の行をチェック
                if i > 0 and indent_level >= 2:
                    prev_line = lines[i - 1].strip()
                    # 前の行が '=' や '(' や ',' で終わっている場合は継続行
                    if prev_line and (prev_line.endswith('=') or prev_line.endswith('(') or
                                     prev_line.endswith(',') or prev_line.endswith(':')):
                        # 継続行なので挿入しない
                        result.append(line)
                        i += 1
                        continue

                # 行が ':' で始まる（インラインアセンブリのオペランドリスト）
                if stripped.startswith(':'):
                    # 継続行なので挿入しない
                    result.append(line)
                    i += 1
                    continue

                # for文の初期化部分は除外
                if not (stripped.startswith('for') and stripped.endswith(');')):
                    # ただし、単純な宣言は除外（typedef, extern, staticなど）
                    if not any(stripped.startswith(kw) for kw in ['typedef', 'extern', 'struct', 'union', 'enum']):
                        should_instrument = True

            # 2. 制御構文（elseは除外済み）
            control_keywords = ['if', 'while', 'for', 'switch', 'do']
            for kw in control_keywords:
                if stripped.startswith(kw + ' ') or stripped.startswith(kw + '('):
                    should_instrument = True
                    break

            # 3. return文
            if stripped.startswith('return'):
                should_instrument = True

            # 4. 既にCOVERAGE_LINE()がある行は除外
            if 'COVERAGE_LINE' in stripped:
                should_instrument = False

            # 5. 単独の開き波括弧は除外
            if stripped == '{' or stripped == '}':
                should_instrument = False

            # 6. caseラベルは除外
            if stripped.startswith('case ') or stripped == 'default:':
                should_instrument = False

            if should_instrument:
                indent = get_indent(line)
                result.append(indent + 'COVERAGE_LINE();')
                # マニフェストに記録（COVERAGE_LINE()自身の行番号）
                inserted_line = len(result)  # 追加したCOVERAGE_LINE()の行番号（1ベース）
                coverage_manifest.append(f"{file_path}:{inserted_line}")

        result.append(line)
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
