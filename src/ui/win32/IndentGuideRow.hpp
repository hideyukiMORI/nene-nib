#pragma once

#include "BodyLayout.hpp"
#include "DevicePixels.hpp"
#include "IndentGuideScale.hpp"
#include "IndentGuides.hpp"
#include "LayoutRect.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 1 行のインデントガイドを描く位置（ADR 0193 の決定 9）。left は行の文字組みの原点の x（本文の欄の
// 左端 ＋ 横の送り・line_area の left）、top と height は最初の画面行（折り返した行の 2
// 行目以降には 引かない）、space は本文の書式の空白の幅（BodyTextLayouts::space_width）、thin と
// thick は線の 太さ（1 DIP と 2 DIP の物理画素）、clip_left と clip_right は本文の欄の左右の端
// （画面の外の段を回さない・#1975 のレビュー R1）。公開 aggregate（CPP-003）。
struct IndentGuideRow
{
    application::IndentGuides guides;
    float left;
    float top;
    float height;
    float space;
    float thin;
    float thick;
    float clip_left;
    float clip_right;
};

// 線の太さ（DIP・見本の 1 DIP と強める段の 2 DIP）。
inline constexpr std::int32_t indent_guide_dips = 1;
inline constexpr std::int32_t strong_indent_guide_dips = 2;

// renderer の行の描画が渡す値から 1 行の位置を組む 1 本（draw_indent_guides と試験が通る）。area は
// line_area（横の送りを含む左端・最初の画面行の上端）、body は枠の配置（行の高さと本文の欄）。
[[nodiscard]] inline IndentGuideRow indent_guide_row(const core::BodyLayout &body,
                                                     const core::LayoutRect &area,
                                                     const application::IndentGuides &guides,
                                                     IndentGuideScale scale) noexcept
{
    return IndentGuideRow{guides,
                          static_cast<float>(area.left),
                          static_cast<float>(area.top),
                          static_cast<float>(body.line_height),
                          scale.space,
                          static_cast<float>(core::to_pixels(indent_guide_dips, scale.dpi)),
                          static_cast<float>(core::to_pixels(strong_indent_guide_dips, scale.dpi)),
                          static_cast<float>(body.content.left),
                          static_cast<float>(body.content.right)};
}
} // namespace nenenib::ui::win32
