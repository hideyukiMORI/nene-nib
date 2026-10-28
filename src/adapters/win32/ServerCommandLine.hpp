#pragma once

#include "ServerFailure.hpp"

#include <expected>
#include <string>
#include <vector>

namespace nenenib::adapters::win32
{
// 言語サーバーの CreateProcessW に渡す命令行（ADR 0160 の決定 2）。先頭は実行ファイルの経路で、
// 各引数は CommandLineToArgvW と C の実行時が同じ値に読み戻す形に引用する（空白・`"`・直前の `\`
// を逃がす）。引数の UTF-8 → UTF-16 は core::to_utf16 の 1 本（CPP-014）。UTF-8
// に直せない引数があれば cannot_start（起こさない）。
[[nodiscard]] std::expected<std::wstring, application::ServerFailure>
server_command_line(const std::wstring &executable, const std::vector<std::string> &arguments);
} // namespace nenenib::adapters::win32
