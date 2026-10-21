#pragma once

#include "Language.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"

#include <cstddef>
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

// 字句器を持つ言語か（段 2・ADR 0156 の決定 14）。持たない言語の frame は lex_line も lex_state も
// 呼ばず、色の区間の状態の cache にも触れない。
[[nodiscard]] bool has_lexer(Language language) noexcept;

// 覚えが無いときに見えている最初の行の手前へ遡る行数（Vim の c_minlines の型・ADR 0156 の決定 8
// と追補 #827）。言語ごとの表はこの 1 つ。行をまたぐ状態の無い JSON は 1、フェンスの長い Markdown
// は 500、C / C++・Python・JavaScript / TypeScript は 200、ほかは 0。Markdown と JS / TS は字句器
// より先に置いた先取りで、has_lexer が偽のうちは使われない。
[[nodiscard]] std::size_t syntax_backfill_lines(Language language) noexcept;
} // namespace nenenib::core
