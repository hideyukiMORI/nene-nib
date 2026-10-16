#pragma once

#include "DisplayForm.hpp"

#include <string_view>

namespace nenenib::core
{
// ステータスバーの右側に出す表示形式の印（ADR 0140 の決定 3）。text は印を出さない（空）。
// 文字列はここ 1 か所で、renderer は書式の別表を持たない。
[[nodiscard]] std::string_view display_form_label(DisplayForm display) noexcept;
} // namespace nenenib::core
