#pragma once

#include "EditMode.hpp"
#include "VimMode.hpp"

#include <string_view>

namespace nenenib::core
{
// ステータスバーに出す現在モードの文字。Vim のときは Vim のモードがそのまま名前になる
// （採用案 第 1 節・D15 / ADR 0012 の決定 10）。
[[nodiscard]] std::string_view mode_label(EditMode mode, VimMode vim) noexcept;
} // namespace nenenib::core
