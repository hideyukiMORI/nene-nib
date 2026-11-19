#pragma once

#include "AutosaveId.hpp"
#include "ContextMenuItem.hpp"
#include "ContextMenuKind.hpp"
#include "ContextMenuMode.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 開いているメニュー（controller が持つ・ADR 0078 の追補 #1051 / #1790）。項目の列と mode は開いた
// ときに決め、開いている間は変えない（閉じる意図の 1 か所が閉じる）。タブのメニューは tab に右
// クリックしたタブの退避番号を、band に開いたときの帯（退避番号の並び）を持ち、帯が変われば閉じる。
// 本文のメニューの tab は無しで band は空。
struct ContextMenuOpen
{
    std::int32_t x;
    std::int32_t y;
    core::ContextMenuKind kind;
    core::ContextMenuMode mode;
    std::vector<core::ContextMenuItem> items;
    std::optional<std::size_t> highlighted;
    std::optional<AutosaveId> tab;
    std::vector<AutosaveId> band;
};
} // namespace nenenib::application
