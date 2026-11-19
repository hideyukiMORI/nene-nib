#pragma once

#include <cstdint>

namespace nenenib::core
{
// 右クリックのメニューの種類（ADR 0078 の追補 #1051 / #1790）。body は本文のメニュー（操作の表の
// 列 context_menu）、tab はタブのメニュー（列 tab_menu）。配置・描画・選び方・閉じ方は同じ部品を
// 使い、項目の列と使えるかの条件だけが種類ごとに違う。
enum class ContextMenuKind : std::uint8_t
{
    body,
    tab
};
} // namespace nenenib::core
