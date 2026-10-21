#pragma once

#include <cstdint>

namespace nenenib::core
{
// 1 行の中で直前に読んだ字句が、次の `/` を何にするか（#833・ADR 0156 の追補）。JavaScript の字句器
// が正規表現リテラルと除算を見分けるのに使う。expression_start は行頭・演算子・`(` `[` `{` `}` `,`
// `;`・`return` などの後で、次の `/` は正規表現の始まり。operand は識別子・数字・文字列・`)` `]`
// の後で、次の `/` は除算。行をまたがない（LexCursor は行ごとに expression_start で始まる）。
enum class LexPrecedent : std::uint8_t
{
    expression_start,
    operand
};
} // namespace nenenib::core
