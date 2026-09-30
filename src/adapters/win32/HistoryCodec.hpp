#pragma once

#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "FileHistoryFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// history.v1 の上限（ADR 0060 の決定 8）。越えたものは書かず、読むときも全体を拒む。行の上限は
// 覚える件数の 10 倍（ほかの版の道具が足した分まで読む余地・それより長いものは壊れている）。
inline constexpr std::size_t maximum_history_bytes = 1024U * 1024U;
inline constexpr std::size_t maximum_history_lines = application::history_limit * 10U;

// 形式（UTF-8・書きは LF・読みは CRLF と BOM も受ける）:
//   version=1
//   <パス>   ← 1 行 1 ファイル・新しい順
// 空行・絶対でないパス・FilePath が拒むものは、一部だけ返さず全体を拒む。
[[nodiscard]] std::expected<application::FileHistory, application::FileHistoryFailure>
decode_history(std::string_view bytes);
[[nodiscard]] std::expected<std::string, application::FileHistoryFailure>
encode_history(const application::FileHistory &history);
} // namespace nenenib::adapters::win32
