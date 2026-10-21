#pragma once

#include "Language.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"

#include <string_view>

namespace nenenib::core
{
// 1 行の字句解析の入口（#815・ADR 0156 の決定 4）。line は改行を含まない 1 行の本文（検証済みの
// UTF-8・CRLF の文書でも `\r` は呼び出し元が除いて渡す）で、in はその行の前の行が渡した状態。
// 字句器を持たない言語では spans が空で out == in。純関数で、割り当ては spans だけ。
[[nodiscard]] LexLine lex_line(Language language, const LexState &in,
                               std::string_view line) noexcept;

// lex_line と同じ状態機械を通して出る状態だけを返す（区間の vector を確保しない）。見えている行の
// 前を遡って状態を求める道（段 2）が使う。lex_line(language, in, line).out と常に等しい。
[[nodiscard]] LexState lex_state(Language language, const LexState &in,
                                 std::string_view line) noexcept;
} // namespace nenenib::core
