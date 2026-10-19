#pragma once

#include <string>

namespace nenenib::adapters::win32
{
// Win32 のファイルの関数へ渡す直前の経路の形（Issue #498）。先に GetFullPathNameW で絶対にし
// （相対・`..`・`/` を畳む）、絶対にした長さが MAX_PATH − 12（フォルダを作れる上限）以上なら
// `\\?\`（UNC は `\\?\UNC\`）を付けて、レジストリの LongPathsEnabled に依らず 32,767 文字まで
// 通す。短ければ絶対にした形を返す。既に `\\?\` か `\\.\` で始まる経路と、絶対にできない経路は
// そのまま返す（後の Win32 の呼び出しが失敗する）。表示と一覧に書く経路はこの形にしない。
// adapters/win32 のファイルの Win32 呼び出しは全部この 1 本を通す。
[[nodiscard]] std::wstring native_path(const std::wstring &path);
} // namespace nenenib::adapters::win32
