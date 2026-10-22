#include "LexLua.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "LexState.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。予約語は Lua 5.4 の 22 語
// から定数の 3 語（`nil` `true` `false`）を除いたもの。
constexpr std::array<std::string_view, 19> lua_keywords{
    "and", "break", "do",  "else", "elseif", "end",    "for",  "function", "goto", "if",
    "in",  "local", "not", "or",   "repeat", "return", "then", "until",    "while"};
constexpr std::array<std::string_view, 3> lua_constants{"false", "nil", "true"};
static_assert(std::ranges::is_sorted(lua_keywords));
static_assert(std::ranges::is_sorted(lua_constants));

constexpr std::string_view operator_characters = "+-*/%^#&~|<>=(){}[];:,.";

// [at, …) を長い括弧の中身として from から閉じまで塗る（長いコメントは comment・長い文字列は
// string）。閉じなければ行末まで塗り、inside の状態で次の行へ。
std::size_t mark_long(LexCursor &cursor, std::size_t at, std::size_t from, const LexState &inside)
{
    const std::string_view line = cursor.line();
    const SyntaxRole role =
        inside.kind == LexKind::block_comment ? SyntaxRole::comment : SyntaxRole::string;
    const std::size_t end = lex_long_close(line, from, inside.delimiter_length);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), role);
        cursor.leave(inside);
        return line.size();
    }
    cursor.mark(at, end, role);
    return end;
}

// ---- 文字列とコメント ----

// `"…"` と `'…'`。閉じなければ行末まで error（`\` と改行の続き・`\z` は限界）。
std::size_t lex_quoted(LexCursor &cursor, std::size_t at)
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

// `--` の後ろが長い括弧の開きなら長いコメント、そうでなければ行末までのコメント。
std::size_t lex_comment(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::optional<std::size_t> level = lex_long_open(line, at + 2U);
    if (level.has_value())
    {
        return mark_long(cursor, at, at + 4U + level.value(),
                         lex_long_state(LexKind::block_comment, level.value()));
    }
    cursor.mark(at, line.size(), SyntaxRole::comment);
    return line.size();
}

// ---- 演算子と数字 ----

// `.5` の数の始まり。`..` の 2 つ目の `.` は連結の演算子の続きで、数を始めない（`x..2`）。
[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    const bool joined = at > 0U && line[at - 1U] == '.';
    return line[at] == '.' && !joined && lex_is_digit(lex_next_of(line, at));
}

// 演算子の連なりを止める所（コメント・長い括弧・`.5`）。
[[nodiscard]] constexpr bool breaks_operator(std::string_view line, std::size_t at) noexcept
{
    const bool comment = line[at] == '-' && lex_next_of(line, at) == '-';
    return comment || lex_long_open(line, at).has_value() || starts_number(line, at);
}

[[nodiscard]] constexpr bool is_hex_digit(char value) noexcept
{
    const char lower = static_cast<char>(value | '\x20');
    return lex_is_digit(value) || (lower >= 'a' && lower <= 'f');
}

// 数字の途中の 1 バイトが数字を続けるか。識別子の文字、`.` は直後が桁（16 進なら 16 進の桁）の
// ときだけ（`1..2` の `..` で切る）、符号は指数（10 進は `e`・16 進は `p`）の直後だけ。
[[nodiscard]] constexpr bool continues_number(std::string_view line, std::size_t at,
                                              bool hex) noexcept
{
    const char value = line[at];
    if (lex_is_word(value))
    {
        return true;
    }
    if (value == '.')
    {
        const char next = lex_next_of(line, at);
        return hex ? is_hex_digit(next) : lex_is_digit(next);
    }
    const char exponent = static_cast<char>(line[at - 1U] | '\x20');
    return (value == '+' || value == '-') && exponent == (hex ? 'p' : 'e');
}

std::size_t lex_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const bool hex = line[at] == '0' && (lex_next_of(line, at) | '\x20') == 'x';
    std::size_t end = at + 1U;
    while (end < line.size() && continues_number(line, end, hex))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::number);
    return end;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (starts_number(line, at))
    {
        return lex_number(cursor, at);
    }
    if (line[at] == '-' && lex_next_of(line, at) == '-')
    {
        return lex_comment(cursor, at);
    }
    const std::optional<std::size_t> level = lex_long_open(line, at);
    if (level.has_value())
    {
        return mark_long(cursor, at, at + 2U + level.value(),
                         lex_long_state(LexKind::raw_string, level.value()));
    }
    std::size_t end = at + 1U;
    while (end < line.size() && operator_characters.contains(line[end]) &&
           !breaks_operator(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// ---- 識別子 ----

[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// `function` の後ろの名前 `a.b:c`（Vim の luaFunction の後ろ）。区切りの `.` `:` は operators、
// 最後の名前は function。名前が無ければ（無名の関数）何も塗らない。
std::size_t mark_function_name(LexCursor &cursor, std::size_t from)
{
    const std::string_view line = cursor.line();
    std::size_t at = skip_spaces(line, from);
    if (at >= line.size() || !lex_is_word(line[at]) || lex_is_digit(line[at]))
    {
        return from;
    }
    std::size_t end = lex_word_end(line, at);
    while (end + 1U < line.size() && (line[end] == '.' || line[end] == ':') &&
           lex_is_word(line[end + 1U]) && !lex_is_digit(line[end + 1U]))
    {
        cursor.mark(end, end + 1U, SyntaxRole::operators);
        at = end + 1U;
        end = lex_word_end(line, at);
    }
    cursor.mark(at, end, SyntaxRole::function);
    return end;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::string_view word = line.substr(at, end - at);
    if (lex_in<lua_constants>(word))
    {
        cursor.mark(at, end, SyntaxRole::constant);
        return end;
    }
    if (!lex_in<lua_keywords>(word))
    {
        return end;
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return word == "function" ? mark_function_name(cursor, end) : end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
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
    if (value == '"' || value == '\'')
    {
        return lex_quoted;
    }
    return operator_characters.contains(value) ? lex_operators : skip_character;
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白・`\`・0x80 以上は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// ---- 行をまたぐ状態の続き ----

// 長い括弧の中で始まる行。delimiter が `=` だけの列でなければ Lua の状態でないので行頭から読む。
std::size_t resume_long(LexCursor &cursor, const LexState &in)
{
    if (!lex_long_level(lex_delimiter(in)))
    {
        return 0U;
    }
    return mark_long(cursor, 0U, 0U, in);
}

// plain で始まる行。行頭の `#!`（shebang・Lua は先頭行の `#` を読み飛ばす）は行末までのコメント
// （#861）。
std::size_t resume_plain(LexCursor &cursor)
{
    const std::string_view line = cursor.line();
    if (!line.starts_with("#!"))
    {
        return 0U;
    }
    cursor.mark(0U, line.size(), SyntaxRole::comment);
    return line.size();
}

std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return resume_plain(cursor);
    case LexKind::block_comment:
    case LexKind::raw_string:
        return resume_long(cursor, in);
    }
    std::unreachable();
}
} // namespace

void lex_lua(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
