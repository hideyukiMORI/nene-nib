#pragma once

#include "BodyLayout.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "VisibleLines.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 窓が application へ送る寸法 VisibleLines を作る 1 本（EditorWindow::body_view が呼ぶ・#1125）。
// 本文の行数は操作中の枠の body、面と補完の行数は窓全体の client の寸法から決める（ADR 0150 の
// 決定 1）。エクスプローラーの面の行数も client の高さから数え、分割の枠の高さには依らない
// （ADR 0162 の追補）。client は窓の client 領域の物理画素（左上が原点）。本文の寸法 geometry は
// 字幅を測ってから呼び出し元が置く（ここでは既定値）。
[[nodiscard]] application::VisibleLines window_lines(const core::BodyLayout &body,
                                                     core::LayoutRect client, std::uint32_t dpi,
                                                     core::FontSize font_size) noexcept;
} // namespace nenenib::ui::win32
