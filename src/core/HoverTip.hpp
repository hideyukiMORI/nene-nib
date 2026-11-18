#pragma once

#include "LayoutRect.hpp"
#include "TabTipMeasure.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 言語サーバーの hover の札（ADR 0190 の決定 6）。文の切り詰めと置き場を決める純関数。測るのと
// 描くのは ui（タブの札の部品 TabTipPaint / paint_tab_tip を使い回す）、出す・消すは application の
// 状態。余白・角丸・端の空きはタブの札の値（TabTip.hpp）と同じ。

// 札の幅の上限（余白を含む・DIP）と行の上限。
inline constexpr std::int32_t hover_tip_max_width_dips = 480;
inline constexpr std::size_t hover_tip_max_lines = 12;

// 札の文。改行は LF にし（CRLF と単独の CR）、末尾の空の行を落とす。行が上限を越えたら上限の
// 行までを残して末尾に「…」を付ける（clipped の 1 本）。utf8 は検証済み。
[[nodiscard]] std::string hover_tip_text(std::string_view utf8);

// 先頭から kept 文字（Vim の 1 文字・ADR 0053）を残し、末尾の空白と改行を落として「…」を付けた
// 文。kept が文字の数以上なら元のまま。折り返した画面行が上限を越えるとき、ui が収まる最も多い
// kept を二分探索で探す（測るのは DirectWrite・切るのはここ）。code point を割らない。
[[nodiscard]] std::string hover_tip_clipped(std::string_view utf8, std::size_t kept);

// 文の幅の上限（物理画素）。札の幅の上限と、置ける領域の幅 − 2 × 端の空きの小さい方から、左右の
// 余白を引いた値。負なら 0。
[[nodiscard]] std::int32_t hover_tip_text_limit(std::int32_t area_width,
                                                std::uint32_t dpi) noexcept;

// 札の矩形（物理画素）。文の大きさに余白を足した大きさで、キャレットの上（空きを挟む）に置き、
// 領域 area の上端からはみ出すならキャレットの下に置く。左はキャレットの左端で、領域の右の端の
// 空きからはみ出すなら左へ寄せ、領域の左端より左へは出さない。
[[nodiscard]] LayoutRect hover_tip_rect(const LayoutRect &caret, const LayoutRect &area,
                                        const TabTipMeasure &text) noexcept;
} // namespace nenenib::core
