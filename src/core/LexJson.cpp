#include "LexJson.hpp"

#include "LexCursor.hpp"
#include "LexScan.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace nenenib::core
{
namespace
{
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。
constexpr std::array<std::string_view, 3> json_constants{"false", "null", "true"};
static_assert(std::ranges::is_sorted(json_constants));

constexpr std::string_view json_operators = "{}[]:,";

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

// `"` から閉じる `"` まで。キーも値も string で、閉じていなければ行末まで error。
std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_closing_quote(line, at);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::error);
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

// 数字の途中の 1 バイトが数字を続けるか。符号は指数の `e` `E` の直後だけ。
[[nodiscard]] constexpr bool continues_number(std::string_view line, std::size_t at) noexcept
{
    const char value = line[at];
    if (lex_is_digit(value) || value == '.' || value == 'e' || value == 'E')
    {
        return true;
    }
    const char exponent = static_cast<char>(line[at - 1U] | '\x20');
    return (value == '+' || value == '-') && exponent == 'e';
}

std::size_t lex_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && continues_number(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::number);
    return end;
}

// `-` は直後が数字のときだけ数字の始まり。ほかは分類しない。
std::size_t lex_minus(LexCursor &cursor, std::size_t at)
{
    return lex_is_digit(lex_next_of(cursor.line(), at)) ? lex_number(cursor, at) : at + 1U;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && json_operators.contains(line[end]))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// true false null は constant。ほかの識別子（裸のキー）は区間なし。
std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    if (lex_in<json_constants>(line.substr(at, end - at)))
    {
        cursor.mark(at, end, SyntaxRole::constant);
    }
    return end;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    if (lex_is_word(value))
    {
        return lex_word;
    }
    if (value == '"')
    {
        return lex_string;
    }
    if (value == '-')
    {
        return lex_minus;
    }
    return json_operators.contains(value) ? lex_operators : skip_character;
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白と 0x80 以上は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();
} // namespace

void lex_json(LexCursor &cursor, const LexState & /*in*/)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = 0; at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
