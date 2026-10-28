#pragma once

#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーの CreateProcessW に渡す命令行（ADR 0160 の決定 2）。先頭は実行ファイルの経路で、
// 各引数は CommandLineToArgvW と C の実行時が同じ値に読み戻す形に引用する（空白・`"`・直前の `\`
// を逃がす）。引数の UTF-8 → UTF-16 は core::to_utf16 の 1 本（CPP-014）。
// 変換できない引数は空の引数になる。
[[nodiscard]] std::wstring server_command_line(const std::wstring &executable,
                                               const std::vector<std::string> &arguments);
} // namespace nenenib::adapters::win32
