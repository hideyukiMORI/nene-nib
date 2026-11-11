#pragma once

#include "BodyLayout.hpp"
#include "GuideRowLayout.hpp"
#include "LayoutRect.hpp"
#include "StartEntry.hpp"
#include "StartRowLayout.hpp"
#include "StartScreenShape.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nenenib::core
{
// 起動画面の配置（物理画素・ADR 0172 の決定 5・採用案
// docs/design/2026-10-28-start-screen-look.md）。 本文の面の中央寄せの 1
// 列で、上から題・最近使ったファイル・ブックマーク・（空の知らせ）・ はじめる・鍵の順。rows
// は選べる行（start_entries の順と同じ位置）、keys は鍵の案内の 3 行（鍵の
// 枠と説明）。見出しは節が無ければ値なし。描画（renderer）とクリックの当たり（窓）が同じ 1 本を
// 読む。公開 aggregate（CPP-003）。
struct StartScreenLayout
{
    LayoutRect column;
    LayoutRect title;
    std::optional<LayoutRect> recent_heading;
    std::optional<LayoutRect> bookmark_heading;
    std::optional<LayoutRect> empty_note;
    LayoutRect start_heading;
    LayoutRect key_heading;
    std::vector<StartRowLayout> rows;
    std::array<GuideRowLayout, 3> keys;
};

// 寸法の採用値（DIP・PR の表に凍結する値）。列の幅・列の左右に残す余白・本文の上端から題までの
// 余白・題の後と節の間の余白・見出しの高さと見出しの後の余白・行の左右の余白・印の升・印と項目の
// 間・鍵の枠の列と説明の間。
inline constexpr std::int32_t start_column_dips = 480;
inline constexpr std::int32_t start_column_margin_dips = 16;
inline constexpr std::int32_t start_top_dips = 36;
inline constexpr std::int32_t start_title_gap_dips = 22;
inline constexpr std::int32_t start_section_gap_dips = 16;
inline constexpr std::int32_t start_heading_dips = 20;
inline constexpr std::int32_t start_heading_gap_dips = 4;
inline constexpr std::int32_t start_row_padding_dips = 6;
inline constexpr std::int32_t start_mark_dips = 18;
inline constexpr std::int32_t start_mark_gap_dips = 8;
inline constexpr std::int32_t start_key_gap_dips = 12;
// 名前と場所の間・「はじめる」の名前と補足の間（Ctrl+P の面の題名と場所の間と同じ 12 DIP・
// 追補 2026-11-11 の 20）。描画が名前の幅を測ってから置く。
inline constexpr std::int32_t start_detail_gap_dips = 12;

// 選べる行の列から節の形を数える。lists_read は履歴とブックマークを読み終えたか（偽なら空の知らせを
// 出さない・ADR 0172 の段 2 の追補 14）。
[[nodiscard]] StartScreenShape start_screen_shape(std::span<const StartEntry> entries,
                                                  bool lists_read) noexcept;

// 本文の枠 body に起動画面を置く。列の幅は 480 DIP で、本文の欄（行番号の右）が狭ければ本文の欄の
// 幅から左右 16 DIP を引いた幅。列は本文の欄の中央。行の高さは本文の行の高さ、題は 1.5 倍。
// key_width は鍵の枠の列の幅（物理画素・描画は測った値、当たりは 0 でよい・鍵の行は選べない）で、
// 64 DIP より狭ければ 64 DIP。鍵の枠の列はほかの行の項目と同じ左端から始まる（追補 2026-11-11 の
// 22）。列の幅が 0 以下なら値なし（描かない・当たらない）。縦は本文の帯の下端を越えてもそのまま
// 置き、描画が帯で切る。
[[nodiscard]] std::optional<StartScreenLayout> start_screen_layout(const BodyLayout &body,
                                                                   std::uint32_t dpi,
                                                                   StartScreenShape shape,
                                                                   std::int32_t key_width);

// 点 (x, y) の下の選べる行の位置（rows の位置）。行の外・見出し・鍵の行・本文の帯の外は値なし。
[[nodiscard]] std::optional<std::size_t> start_screen_row_at(const StartScreenLayout &layout,
                                                             const BodyLayout &body, std::int32_t x,
                                                             std::int32_t y) noexcept;
} // namespace nenenib::core
