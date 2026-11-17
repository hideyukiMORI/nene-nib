#pragma once

#include "ContextMenuItem.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuShape.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::application
{
// 開いているメニュー（controller が持つ・ADR 0078 の追補 #1051）。項目の列と mode は開いたときに
// 決め、開いている間は変えない（閉じる意図の 1 か所が閉じる）。
struct ContextMenuOpen
{
    std::int32_t x;
    std::int32_t y;
    core::ContextMenuMode mode;
    std::array<core::ContextMenuItem, core::context_menu_size> items;
    std::optional<std::size_t> highlighted;
};
} // namespace nenenib::application
