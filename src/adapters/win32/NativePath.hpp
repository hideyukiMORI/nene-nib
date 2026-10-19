#pragma once

#include <string>

namespace nenenib::adapters::win32
{
// Win32 のファイルの関数へ渡す直前の経路の形（Issue #498）。長さが MAX_PATH − 12（フォルダを
// 作れる上限）以上なら GetFullPathNameW で絶対にして `\\?\`（UNC は `\\?\UNC\`）を付け、
// レジストリの LongPathsEnabled に依らず 32,767 文字まで通す。短い経路・既に `\\?\` か `\\.\`
// で始まる経路・絶対にできない経路はそのまま返す。表示と一覧に書く経路はこの形にしない。
// adapters/win32 のファイルの Win32 呼び出しは全部この 1 本を通す。
[[nodiscard]] std::wstring native_path(const std::wstring &path);
} // namespace nenenib::adapters::win32
