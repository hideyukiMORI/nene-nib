#pragma once

#include "PaletteScope.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// 探して見る面のプレビュー欄の置き方（ADR 0189 の決定 1）。状態と設定（settings.v5 の鍵
// `finder_preview`）に持つ値で、窓全体で 1 つ。automatic は窓の大きさで right → below → off の
// 入る形を選ぶ。描く形（palette_preview_drawn の答え）は right / below / off のどれかで、
// automatic にはならない。値が増えたら写す switch が落ちる（CPP-002）。
enum class FinderPreview : std::uint8_t
{
    automatic,
    right,
    below,
    off
};

// 窓の行の上限（ADR 0189 の決定 4）。手元の本文から作る窓も、P2 の worker が読む窓も同じ数。
inline constexpr std::size_t finder_preview_line_limit = 40;
// 全文検索の当たりの窓は、当たりの行の前のこの行数から始める（決定 4）。
inline constexpr std::size_t finder_preview_context_lines = 7;
// 窓の 1 行のバイトの上限（決定 4・コードポイントを割らずに切る）。
inline constexpr std::size_t finder_preview_line_bytes = 512;

// 設定と `:set finderpreview=` の名前（`auto` `right` `below` `off`）。
[[nodiscard]] std::string_view finder_preview_name(FinderPreview preview) noexcept;
// 名前の逆引き。表に無い名前は値なし。
[[nodiscard]] std::optional<FinderPreview> finder_preview_named(std::string_view name) noexcept;
// 切り替えの次の値（決定 3）。順は right → below → off → right で、選んだ値に依らず今描いている
// 形 drawn の次（手で right を選んでいて細い窓で below に落ちていれば次は off・押せば見た目が必ず
// 変わる）。drawn に automatic は来ない前提で、来たら off と同じく right。
[[nodiscard]] FinderPreview finder_preview_cycled(FinderPreview drawn) noexcept;
// 出どころがプレビュー欄を持つか（決定 2）。ファイルを指す候補の出どころ（タブ・ブックマーク・
// 履歴・同じフォルダ・全文検索の当たりと、それらを混ぜた files）は持ち、設定 `:` と操作の一覧 `?`
// は持たない（今の面の形）。出どころごとの答えはここの 1 か所。
[[nodiscard]] bool palette_previews(PaletteScope scope) noexcept;
// 面が開いている出どころでの置き方（欄を持たない出どころは off）。配置 palette_layout へ渡す値。
[[nodiscard]] FinderPreview palette_placement(PaletteScope scope, FinderPreview chosen) noexcept;
} // namespace nenenib::core
