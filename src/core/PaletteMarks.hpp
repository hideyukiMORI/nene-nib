#pragma once

#include "DisplayLanguage.hpp"
#include "DisplayText.hpp"
#include "PaletteMark.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 出どころの記号の表（ADR 0060 の決定 2）。載せるのは実装した出どころだけで、出どころを足すときは
// ここに 1 行を足す。表に無い記号で始まる入力は files のただの検索の文字。
inline constexpr std::array<PaletteMark, 6> palette_marks{{
    {'#', PaletteScope::tabs, UiMessage::palette_mark_tabs},
    {'*', PaletteScope::bookmarks, UiMessage::label_bookmarks},
    {'@', PaletteScope::history, UiMessage::label_history},
    {'/', PaletteScope::folder, UiMessage::palette_mark_folder},
    {':', PaletteScope::commands, UiMessage::palette_mark_settings},
    {'?', PaletteScope::operations, UiMessage::palette_mark_operations},
}};

// 入力の先頭の 1 文字を表で引く。表にあればその出どころと残りの文字、無ければ files と入力の全体。
// 呼び出し元で先頭の文字を見ない。
[[nodiscard]] PaletteQuery palette_query_of(std::string_view input) noexcept;

// 出どころを同じ表で記号へ戻す。files の先頭の記号は空白を前置して検索文字として残す。
[[nodiscard]] std::string palette_input_for(const PaletteQuery &query);

// 検索欄の案内（`# タブ　* ブックマーク　@ 履歴　/ フォルダ　: 設定　?
// 操作`）。記号と名前の間は半角の空白、項目の間は表の区切り palette_mark_separator（日本語は全角の
// 空白 U+3000）。名前と区切りは language の文（#1019）。
[[nodiscard]] DisplayText palette_mark_hint(DisplayLanguage language);

// 案内の文（palette_mark_hint）の中で、各組（記号 ＋ 名前）が始まるバイトの位置（表の順・#1640）。
// 欄が狭いとき ui が組の単位で前を落とすために、組の境目の幅を測る所を渡す。
[[nodiscard]] std::array<std::size_t, palette_marks.size()>
palette_mark_hint_starts(DisplayLanguage language);
} // namespace nenenib::core
