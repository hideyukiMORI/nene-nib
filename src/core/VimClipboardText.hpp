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
} // namespace nenenib::core
