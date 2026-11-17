#pragma once

#include "DisplayLanguage.hpp"
#include "FilePath.hpp"
#include "LayoutRect.hpp"
#include "TabTipMeasure.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// タブに載せて一定時間で出す札（#1789・ADR 0056 の #1789 の追補）。中身・真ん中の切り方・置き場
// を決める純関数。測るのと描くのは ui、出す・消すは application の状態。

// 札の寸法（DIP・設計席の決め）。左右と上下の余白・帯との空き・角丸・窓の左右の端の空き（幅の上限は
// 窓の幅 − 2 × 8 = 16 DIP）。
inline constexpr std::int32_t tab_tip_padding_x_dips = 10;
inline constexpr std::int32_t tab_tip_padding_y_dips = 6;
inline constexpr std::int32_t tab_tip_gap_dips = 4;
inline constexpr std::int32_t tab_tip_radius_dips = 6;
inline constexpr std::int32_t tab_tip_edge_dips = 8;

// 札の対象のタブ（帯の位置）。タブの本体と × の上は同じタブで、ほかの要素と帯の外は値なし。
// 載せた要素がこの値で変わったときだけ、待ちを数え直す。
[[nodiscard]] std::optional<std::size_t>
tab_tip_tab(const std::optional<TitleBarTarget> &hovered) noexcept;

// 札の文。経路があればフルパス（区切りは `\`）、無題は「無題」の今の言語の文。
[[nodiscard]] std::string tab_tip_text(const std::optional<FilePath> &path,
                                       DisplayLanguage language);

// 文の文字の数。1 文字は Vim の 1 文字（code point と直後の幅 0 の列・ADR 0053）で、結合文字を
// 前の文字から離さない。utf8 は検証済み。
[[nodiscard]] std::size_t tab_tip_characters(std::string_view utf8) noexcept;

// 真ん中の dropped 文字を「…」1 つに替えた文。残す文字は前に半分（切り捨て）・後ろに残り（ファイル
// 名の側を 1 文字多く残す）。0 なら元のまま、文字の数以上なら「…」だけ。code point を割らない。
[[nodiscard]] std::string middle_clipped(std::string_view utf8, std::size_t dropped);

// 文の幅の上限（物理画素）。窓の幅 − 2 × 端の空き − 2 × 左右の余白で、負なら 0。
[[nodiscard]] std::int32_t tab_tip_text_limit(std::int32_t window_width,
                                              std::uint32_t dpi) noexcept;

// 札の矩形（物理画素）。文の大きさに余白を足した大きさで、左はタブの左端（帯の送りで左へ隠れた
// 分はタブの領域の左端）、上は帯の下端 + 空き。窓の右の端の空きからはみ出すなら左へ寄せ、0 より
// 左へは出さない。
[[nodiscard]] LayoutRect tab_tip_rect(const TitleBarLayout &layout, std::size_t tab,
                                      const TabTipMeasure &text) noexcept;
} // namespace nenenib::core
