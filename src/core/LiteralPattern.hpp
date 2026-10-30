#pragma once

#include "VimSearchDirection.hpp"

#include <string>
#include <string_view>

namespace nenenib::core
{
// 通常モードの検索語を Vim の照合器 VimPattern の文字列にする純関数（ADR 0174 の
// 決定 4）。`magic` の特殊文字 `\` `/` `.` `*` `[` `~` `^` `$` の前に `\` を置き、
// 後ろ向き（direction が backward）なら区切りの `?` も逃がす（VimPattern::parse は
// 逃がしていない区切りを offset として断る）。ほかのバイトはそのまま写す（多バイト
// 文字のバイトは ASCII と重ならない）。照合は同じ VimPattern の 1 本で、照合器を
// 2 本にしない（ARC-012）。`*` `#` の語の検索（Vim の nv_ident）も同じ 1 本を呼ぶ。
[[nodiscard]] std::string literal_pattern(std::string_view text, VimSearchDirection direction);
} // namespace nenenib::core
