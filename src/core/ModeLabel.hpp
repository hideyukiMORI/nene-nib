#pragma once

#include "EditMode.hpp"

#include <string_view>

namespace nenenib::core
{
// ステータスバーに出す現在モードの文字。Vim エンジンが入るまで vim 側は NORMAL 固定（ADR 0008）。
[[nodiscard]] std::string_view mode_label(EditMode mode) noexcept;
} // namespace nenenib::core
