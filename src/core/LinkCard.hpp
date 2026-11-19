#pragma once

#include "DisplayLanguage.hpp"
#include "LayoutRect.hpp"
#include "LinkCardTexts.hpp"
#include "LinkDestination.hpp"
#include "LinkKind.hpp"
#include "PreviewBlock.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::core
{
// プレビューのリンクの札（施主決定 D105・D106・#1869 の段 L2）の純関数。札の状態は application、
// 測るのと描くのは ui。寸法は採用案（D:\NeNeNib\design\link-20261119 の Card）の DIP。

// 札の幅の上限・内側の余白・角丸・リンクとの間・領域の端の空き。
inline constexpr std::int32_t link_card_max_width_dips = 400;
inline constexpr std::int32_t link_card_padding_x_dips = 16;
inline constexpr std::int32_t link_card_padding_y_dips = 14;
inline constexpr std::int32_t link_card_radius_dips = 8;
inline constexpr std::int32_t link_card_gap_dips = 4;
inline constexpr std::int32_t link_card_edge_dips = 8;
// 題の下・宛先の枠の内側（上下・左右）・開き方の 1 行の上・ボタンの行の上の空き。
inline constexpr std::int32_t link_card_title_gap_dips = 6;
inline constexpr std::int32_t link_card_target_padding_y_dips = 6;
inline constexpr std::int32_t link_card_target_padding_x_dips = 8;
inline constexpr std::int32_t link_card_target_radius_dips = 4;
inline constexpr std::int32_t link_card_way_gap_dips = 8;
inline constexpr std::int32_t link_card_row_gap_dips = 14;
// ボタンの高さ・最小の幅・左右の余白・ボタンの間・角丸。
inline constexpr std::int32_t link_card_button_height_dips = 32;
inline constexpr std::int32_t link_card_button_min_width_dips = 72;
inline constexpr std::int32_t link_card_button_padding_dips = 14;
inline constexpr std::int32_t link_card_button_gap_dips = 8;
inline constexpr std::int32_t link_card_button_radius_dips = 6;

// 要素 block の text の中のバイト位置 offset を含むリンク（[begin, end)）の links の中の番号。
// 当たらなければ nullopt。
[[nodiscard]] std::optional<std::size_t> preview_link_at(const PreviewBlock &block,
                                                         std::size_t offset) noexcept;

// 種類ごとの札の文（題・開き方・鍵の案内・ボタン）。鍵の名前は訳さない。heading は段 L3
// まで札を出さないが、閉じた選択肢なので refused と同じ文を返す。
[[nodiscard]] LinkCardTexts link_card_texts(LinkKind kind, DisplayLanguage language);

// 宛先の綴りのうち太字で見せる範囲 [first, second)（バイト位置）。web は `://` の後ろのホスト
// （次の `/` `?` `#` の前まで）、mail は `mailto:` の後ろの宛名（`?` の前まで）、document と
// refused は最後の `\` か `/` の後ろ（refused でも `://` があればホスト）。空の範囲もある。
[[nodiscard]] std::pair<std::size_t, std::size_t>
link_card_emphasis(const LinkDestination &destination) noexcept;

// 札の幅（物理画素）。上限と、領域の幅 − 2 × 端の空きの小さい方（負なら 0）。
[[nodiscard]] std::int32_t link_card_width(std::int32_t area_width, std::uint32_t dpi) noexcept;

// 札の矩形（物理画素）。リンクの矩形 link のすぐ下（空きを挟む）に置き、領域 area の下端から
// はみ出すならリンクの上に置く（上にも入らなければ下端へ寄せ、上端より上へは出さない）。左は
// リンクの左端で、領域の右の端の空きからはみ出すなら左へ寄せ、左の端の空きより左へは出さない。
[[nodiscard]] LayoutRect link_card_rect(const LayoutRect &link, const LayoutRect &area,
                                        std::pair<std::int32_t, std::int32_t> size,
                                        std::uint32_t dpi) noexcept;
} // namespace nenenib::core
