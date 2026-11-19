#pragma once

#include "MarkdownDocument.hpp"
#include "PreviewTableBand.hpp"
#include "PreviewTableSpan.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace nenenib::ui::win32
{
// 枠より広い表の横の送りの寸法（#1883・施主決定 D107 の案 A・ADR 0151 の 2026-11-19 の追補）。
// 物理画素と DIP の写しはここの 1 か所で、描く所（PreviewPainter）・当たり（PreviewPane）・高さの
// 部分和（PreviewBlockTops）が同じ関数を読む。送りの量と端・つまみは core の
// PreviewTableScroll（DIP）。

// 表の行の横の寸法。columns は列の中身の幅、frame_width は枠の幅、indent は行の字下げ。
[[nodiscard]] PreviewTableBand preview_table_band(std::span<const float> columns, float frame_width,
                                                  float indent, std::uint32_t dpi) noexcept;
// 寸法の DIP（枠は切り捨て・表は切り上げ。意図に添える値と描く値は同じこの 1 本）。
[[nodiscard]] core::PreviewTableSpan preview_table_span(PreviewTableBand band,
                                                        std::uint32_t dpi) noexcept;
// 枠より広い表か（棒を出し、送る）。
[[nodiscard]] bool preview_table_overflows(PreviewTableBand band, std::uint32_t dpi) noexcept;
// 送りの量の列 scrolls（表の通し番号ごとの DIP）の表 table の量（無ければ 0）。
[[nodiscard]] std::int32_t preview_table_offset(std::span<const std::int32_t> scrolls,
                                                std::size_t table) noexcept;
// 送りの量 offset（DIP）を端に寄せて物理画素にした、升を左へずらす量（収まる表は 0）。
[[nodiscard]] float preview_table_shift(PreviewTableBand band, std::int32_t offset,
                                        std::uint32_t dpi) noexcept;
// DIP の溝の上の位置 → 物理画素（溝の左端から・溝の幅 band.view に比例して写す）と、その逆。
[[nodiscard]] float preview_table_track_pixels(PreviewTableBand band, std::int32_t dips,
                                               std::uint32_t dpi) noexcept;
[[nodiscard]] std::int32_t preview_table_track_dips(PreviewTableBand band, float pixels,
                                                    std::uint32_t dpi) noexcept;
// index の要素（表の行）が表の最後の行か（次の要素が同じ表の続く行でない）。
[[nodiscard]] bool preview_table_closes(const core::MarkdownDocument &document,
                                        std::size_t index) noexcept;
// 棒の高さ（6 DIP）・最後の行と棒の間（4
// DIP）・棒の分の高さ（その和・広い表の最後の行にだけ足す）。
[[nodiscard]] float preview_table_bar_height(std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_table_bar_gap(std::uint32_t dpi) noexcept;
[[nodiscard]] float preview_table_bar_extent(std::uint32_t dpi) noexcept;
} // namespace nenenib::ui::win32
