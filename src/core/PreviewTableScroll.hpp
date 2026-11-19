#pragma once

#include "PreviewTableBarPart.hpp"
#include "PreviewTableSpan.hpp"
#include "PreviewTableThumb.hpp"

#include <cstdint>

namespace nenenib::core
{
// 枠より広い表の横の送り（#1883・施主決定 D107 の案 A・ADR 0151 の 2026-11-19 の追補）。値は
// どれも DIP で、送りの量は表の左端からの量（0 から limit まで）。ui の描画・当たりと application
// の送りの量が同じ関数を読む。

// つまみの長さの下限（DIP）。
constexpr std::int32_t preview_table_thumb_minimum = 24;

// 表が枠より広いか（棒を出すか）。
[[nodiscard]] bool preview_table_wide(PreviewTableSpan span) noexcept;
// 送りの量の上限（表の幅 − 枠の幅・収まる表は 0）。
[[nodiscard]] std::int32_t preview_table_scroll_limit(PreviewTableSpan span) noexcept;
// 送りの量 offset を 0 から上限に収める（幅が変わって端を越えた送りは端へ寄せる）。
[[nodiscard]] std::int32_t preview_table_scroll_clamped(std::int32_t offset,
                                                        PreviewTableSpan span) noexcept;
// 溝（枠の幅）の中のつまみ。長さは「枠 ÷ 表の幅」に比例し、下限は preview_table_thumb_minimum
// （溝より長くしない）。
[[nodiscard]] PreviewTableThumb preview_table_thumb(std::int32_t offset,
                                                    PreviewTableSpan span) noexcept;
// 溝の左端から x の点が、つまみの前・つまみ・つまみの後ろのどれか。
[[nodiscard]] PreviewTableBarPart preview_table_bar_part(std::int32_t offset, std::int32_t x,
                                                         PreviewTableSpan span) noexcept;
// 溝のつまみの前（後ろ）を押したときの送りの量（1 枠ぶん左（右）・Windows の標準）。つまみの上は
// そのまま。
[[nodiscard]] std::int32_t preview_table_scroll_paged(std::int32_t offset, PreviewTableBarPart part,
                                                      PreviewTableSpan span) noexcept;
// つかんだときの送りの量 from から、ポインタが溝に沿って moved だけ動いたときの送りの量（つまみが
// ポインタに付いて動く）。
[[nodiscard]] std::int32_t preview_table_scroll_dragged(std::int32_t from, std::int32_t moved,
                                                        PreviewTableSpan span) noexcept;
} // namespace nenenib::core
