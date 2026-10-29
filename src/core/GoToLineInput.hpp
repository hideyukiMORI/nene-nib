#pragma once

#include "GoToLineTarget.hpp"

#include <optional>
#include <string_view>

namespace nenenib::core
{
// 「行: 」の入力行の文字列 → 行と桁（ADR 0174 の決定 8）。受けるのは ASCII の
// 10 進の `行` と `行:桁` だけで、全角の数字・空白・符号・空の欄は値なし（呼ぶ側が
// 「行番号を入れてください」を出す）。
[[nodiscard]] std::optional<GoToLineTarget> go_to_line_target(std::string_view text);
} // namespace nenenib::core
