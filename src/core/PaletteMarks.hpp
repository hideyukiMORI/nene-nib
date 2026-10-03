#pragma once

#include "DisplayText.hpp"
#include "PaletteMark.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"

#include <array>
#include <string_view>

namespace nenenib::core
{
// 出どころの記号の表（ADR 0060 の決定 2）。載せるのは実装した出どころだけで、出どころを足すときは
// ここに 1 行を足す。表に無い記号で始まる入力は files のただの検索の文字。
inline constexpr std::array<PaletteMark, 5> palette_marks{{
    {'#', PaletteScope::tabs, "タブ"},
    {'*', PaletteScope::bookmarks, "ブックマーク"},
    {'@', PaletteScope::history, "履歴"},
    {'/', PaletteScope::folder, "フォルダ"},
    {':', PaletteScope::commands, "設定"},
}};

// 入力の先頭の 1 文字を表で引く。表にあればその出どころと残りの文字、無ければ files と入力の全体。
// 呼び出し元で先頭の文字を見ない。
[[nodiscard]] PaletteQuery palette_query_of(std::string_view input) noexcept;

// 検索欄の案内（`# タブ　* ブックマーク　@ 履歴　/ フォルダ　:
// 設定`）。記号と名前の間は半角の空白、項目の間は全角の空白 U+3000。
[[nodiscard]] DisplayText palette_mark_hint();
} // namespace nenenib::core
