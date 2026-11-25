#pragma once

#include "DisplayLanguage.hpp"
#include "DisplayText.hpp"
#include "PaletteMark.hpp"
#include "PaletteMarkHint.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// 出どころの記号の表（ADR 0060 の決定 2）。載せるのは実装した出どころだけで、出どころを足すときは
// ここに 1 行を足す。表に無い記号で始まる入力は files のただの検索の文字。最後の欄は検索欄の案内に
// 出すか（ADR 0060 の 2026-11-22 の追補）。
inline constexpr std::array<PaletteMark, 11> palette_marks{{
    {'#', PaletteScope::tabs, UiMessage::palette_mark_tabs, PaletteMarkHint::always},
    {'*', PaletteScope::bookmarks, UiMessage::label_bookmarks, PaletteMarkHint::always},
    {'@', PaletteScope::history, UiMessage::label_history, PaletteMarkHint::always},
    {'/', PaletteScope::folder, UiMessage::palette_mark_folder, PaletteMarkHint::always},
    {':', PaletteScope::commands, UiMessage::palette_mark_settings, PaletteMarkHint::always},
    {'?', PaletteScope::operations, UiMessage::palette_mark_operations, PaletteMarkHint::always},
    {'!', PaletteScope::project_hits, UiMessage::palette_mark_project, PaletteMarkHint::always},
    // 開いているタブの診断（#1939）。`!` は全文検索が使っているので、警告の三角に見える `^`。
    // 頼まなくても中身があるので、LSP がオンなら案内に出す。
    {'^', PaletteScope::diagnostics, UiMessage::palette_mark_diagnostics,
     PaletteMarkHint::language_server},
    // 言語サーバーの参照（#1947・ADR 0191 の決定 4）。C++ の参照の印の `&`。入口は鍵（`grr` /
    // Shift+F12）なので案内に出さない。
    {'&', PaletteScope::references, UiMessage::palette_mark_references, PaletteMarkHint::never},
    // 言語サーバーの記号（#1955・ADR 0191 の決定 5）。Vim の Ex で今のファイルの `%`。入口は鍵
    // （`gO`）なので案内に出さない。
    {'%', PaletteScope::symbols, UiMessage::palette_mark_symbols, PaletteMarkHint::never},
    // 言語サーバーのクイックフィックス（#2005・ADR 0194 の決定 9・hide に決めてほしいこと 3 の仮の
    // 決め）。Vim の「整える」の `=`。入口は鍵（Ctrl+. / `gra`）なので案内に出さない。
    {'=', PaletteScope::code_actions, UiMessage::palette_mark_code_actions, PaletteMarkHint::never},
}};

// 入力の先頭の 1 文字を表で引く。表にあればその出どころと残りの文字、無ければ files と入力の全体。
// 呼び出し元で先頭の文字を見ない。
[[nodiscard]] PaletteQuery palette_query_of(std::string_view input) noexcept;

// 出どころを同じ表で記号へ戻す。files の先頭の記号は空白を前置して検索文字として残す。
[[nodiscard]] std::string palette_input_for(const PaletteQuery &query);

// 案内に出す記号の行（表の順）。language_server は LSP がオン（設定の lsp）か。案内の文と組の
// 境目はこれから作り、呼び出し元で記号を見て分けない。
[[nodiscard]] std::vector<PaletteMark> palette_hinted_marks(bool language_server);

// 検索欄の案内（`# タブ　* ブックマーク　@ 履歴　/ フォルダ　: 設定　? 操作　!
// 全文　^ 診断`）。出すのは palette_hinted_marks の記号だけ。記号と名前の間は半角の空白、項目の間は
// 表の区切り palette_mark_separator（日本語は全角の 空白 U+3000）。名前と区切りは language の文
// （#1019）。
[[nodiscard]] DisplayText palette_mark_hint(DisplayLanguage language, bool language_server);

// 案内の文（palette_mark_hint）の中で、各組（記号 ＋ 名前）が始まるバイトの位置
// （palette_hinted_marks の順・#1640）。欄が狭いとき ui
// が組の単位で前を落とすために、組の境目の幅を 測る所を渡す。
[[nodiscard]] std::vector<std::size_t> palette_mark_hint_starts(DisplayLanguage language,
                                                                bool language_server);
} // namespace nenenib::core
