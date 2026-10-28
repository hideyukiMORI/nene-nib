#pragma once

#include "VimRegister.hpp"

#include <string_view>

namespace nenenib::core
{
// OS のクリップボードの本文 → `"+` `"*` の写し（ADR 0051 の決定 4）。この 1 本だけが写す
// （ARC-001）。`\r\n` を `\n` に畳み、単独の `\r` は文字として残す。畳んだ後の末尾が `\n` なら
// 行単位、そうでなければ文字単位（本物の Vim 9.1 の実測）。矩形にはしない。空文字列は空の
// 文字単位（貼るものが無いので `p` は refused）。文字列の純関数で OS に触れない（ARC-007）。
[[nodiscard]] VimRegister vim_register_of_clipboard(std::string_view text);

// レジスタ → OS へ出す本文の LF の形（ADR 0051 の決定 7 と追補（#848）の決定 G）。この 1 本だけが
// 作る（ARC-001）。矩形は行を改行で繋いだ本文（engine の値のまま）。controller は文書の改行に直して
// 出し、無名の同期は読んだ本文（CRLF を畳んだ後）とこの形を比べて自分の書いた本文かを決める。
[[nodiscard]] std::string_view vim_clipboard_text_of(const VimRegister &value) noexcept;
} // namespace nenenib::core
