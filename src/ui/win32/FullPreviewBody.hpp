#pragma once

#include "BodyLayout.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 全面プレビューを描く本文の面（ADR 0155 の改訂 15）。area は面（エクスプローラー）を除いた
// client で、1 枠の配置の本文の帯を返す。枠を分けていても配置は変えず（PaneLayouts の原点は
// そのまま）、描く側が帯の全部（gutter の列も含む）をプレビューに使う。renderer と試験が同じ 1 本を
// 引く。
[[nodiscard]] core::BodyLayout full_preview_body(const core::LayoutRect &area, std::uint32_t dpi,
                                                 core::FontSize font_size) noexcept;
} // namespace nenenib::ui::win32
