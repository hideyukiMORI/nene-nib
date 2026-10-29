#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// 通常モードの検索語を Vim の照合器 VimPattern の文字列にする純関数（ADR 0174 の
// 決定 4）。`magic` の特殊文字 `\` `/` `.` `*` `[` `~` `^` `$` の前に `\` を置き、
// ほかのバイトはそのまま写す（多バイト文字のバイトは ASCII と重ならない）。照合は
// 同じ VimPattern の 1 本で、照合器を 2 本にしない（ARC-012）。`*` `#` の語の検索
// （Vim の nv_ident）も同じ逃がし方を使う。
[[nodiscard]] std::string literal_pattern(std::string_view text);
} // namespace nenenib::core
