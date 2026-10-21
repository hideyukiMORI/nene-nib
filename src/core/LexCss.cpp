#include "LexCss.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

constexpr std::string_view css_operators = "{}:;,";
constexpr std::string_view important = "important";

// 名前の文字（`-` と数字と 0x80 以上を含む）。
[[nodiscard]] constexpr bool is_ident(char value) noexcept
{
    return lex_is_word(value) || value == '-' || static_cast<unsigned char>(value) >= 0x80U;
}

[[nodiscard]] constexpr std::size_t ident_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && is_ident(line[at]))
    {
        ++at;
    }
    return at;
}

[[nodiscard]] constexpr bool is_hex(char value) noexcept
{
    const char lower = static_cast<char>(value | '\x20');
    return lex_is_digit(value) || (lower >= 'a' && lower <= 'f');
}

// at が数字の始まりか（数字・`.5`・`-1` `+.5` の符号）。
[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    const char value = line[at];
    const char next = lex_next_of(line, at);
    if (lex_is_digit(value) || value == '.')
    {
        return lex_is_digit(value) || lex_is_digit(next);
    }
    const bool sign = value == '-' || value == '+';
    const bool fraction = next == '.' && lex_is_digit(lex_next_of(line, at + 1U));
    return sign && (lex_is_digit(next) || fraction);
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

// ---- コメントと文字列 ----

// begin から、from の後ろの `*/` までがコメント。閉じなければ行末までで次の行へ block_comment。
std::size_t mark_comment(LexCursor &cursor, std::size_t begin, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t close = lex_find(line, from, "*/");
    if (close == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::comment);
        cursor.leave(lex_entered(LexKind::block_comment, {}));
        return line.size();
    }
    cursor.mark(begin, close + 2U, SyntaxRole::comment);
    return close + 2U;
}

std::size_t lex_slash(LexCursor &cursor, std::size_t at)
{
    return lex_next_of(cursor.line(), at) == '*' ? mark_comment(cursor, at, at + 2U) : at + 1U;
}

// `"…"` `'…'` は `\` で次の 1 バイトを飛ばす。閉じていなければ行末まで error。
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

// ---- 数字と色と印 ----

// 数字の本体（数字と `.`）の後ろに `%` か単位の英字が続く。
std::size_t lex_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && (lex_is_digit(line[end]) || line[end] == '.'))
    {
        ++end;
    }
    end = end < line.size() && line[end] == '%' ? end + 1U : lex_word_end(line, end);
    cursor.mark(at, end, SyntaxRole::number);
    return end;
}

// `#` の後ろの名前が 16 進の 3 / 4 / 6 / 8 桁なら色で constant。ほか（`#id` のセレクタ）は
// 区間なし。
std::size_t lex_hash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = ident_end(line, at + 1U);
    const std::string_view digits = line.substr(at + 1U, end - at - 1U);
    const std::size_t length = digits.size();
    const bool sized = length == 3U || length == 4U || length == 6U || length == 8U;
    if (sized && std::ranges::all_of(digits, is_hex))
    {
        cursor.mark(at, end, SyntaxRole::constant);
    }
    return end;
}

// `@media` `@import` などの at-rule は keyword。
std::size_t lex_at_rule(LexCursor &cursor, std::size_t at)
{
    const std::size_t end = ident_end(cursor.line(), at + 1U);
    if (end == at + 1U)
    {
        return end;
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

// `!important`（`!` と名前の間の空白も・大小を問わない）は keyword。ほかの `!` は区間なし。
std::size_t lex_bang(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t name = at + 1U;
    while (name < line.size() && lex_is_space(line[name]))
    {
        ++name;
    }
    const std::size_t end = ident_end(line, name);
    const auto folded = [](char left, char right)
    { return static_cast<char>(left | '\x20') == right; };
    if (!std::ranges::equal(line.substr(name, end - name), important, folded))
    {
        return at + 1U;
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && css_operators.contains(line[end]))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// ---- 名前 ----

// at の前が（空白を飛ばして）行頭か `{` `;` か。
[[nodiscard]] constexpr bool starts_declaration(std::string_view line, std::size_t at) noexcept
{
    while (at > 0U && lex_is_space(line[at - 1U]))
    {
        --at;
    }
    return at == 0U || line[at - 1U] == '{' || line[at - 1U] == ';';
}

// [at, end) の名前が宣言の始まりにあり、後ろに `:` が続き、その先が `;` `}` 行末より前に `{` を
// 持たない（`a:hover {` はセレクタ）ならプロパティ名。
[[nodiscard]] constexpr bool is_property(std::string_view line, std::size_t at,
                                         std::size_t end) noexcept
{
    std::size_t colon = end;
    while (colon < line.size() && lex_is_space(line[colon]))
    {
        ++colon;
    }
    if (colon >= line.size() || line[colon] != ':' || !starts_declaration(line, at))
    {
        return false;
    }
    std::size_t stop = colon;
    while (stop < line.size() && line[stop] != '{' && line[stop] != ';' && line[stop] != '}')
    {
        ++stop;
    }
    return stop == line.size() || line[stop] != '{';
}

std::size_t lex_ident(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = ident_end(line, at);
    if (is_property(line, at, end))
    {
        cursor.mark(at, end, SyntaxRole::type);
    }
    return end;
}

// `-` `+` `.` は数字の始まりなら数字、`-` はほかでは名前（`-webkit-` `--var`）。
std::size_t lex_sign(LexCursor &cursor, std::size_t at)
{
    if (starts_number(cursor.line(), at))
    {
        return lex_number(cursor, at);
    }
    return cursor.line()[at] == '-' ? lex_ident(cursor, at) : at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 9> marks{{{'"', lex_string},
                                                                {'\'', lex_string},
                                                                {'/', lex_slash},
                                                                {'#', lex_hash},
                                                                {'@', lex_at_rule},
                                                                {'!', lex_bang},
                                                                {'-', lex_sign},
                                                                {'+', lex_sign},
                                                                {'.', lex_sign}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return css_operators.contains(value) ? lex_operators : skip_character;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    return lex_is_word(value) || static_cast<unsigned char>(value) >= 0x80U ? lex_ident
                                                                            : marked_handler(value);
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// ブロックコメントの中で始まる行は、閉じまでを読んでから続ける。raw_string は CSS の状態でない
// ので行頭から読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
    case LexKind::raw_string:
        return 0U;
    case LexKind::block_comment:
        return mark_comment(cursor, 0U, 0U);
    }
    std::unreachable();
}
} // namespace

void lex_css(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
