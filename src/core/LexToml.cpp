#include "LexToml.hpp"

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

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。
constexpr std::array<std::string_view, 2> toml_constants{"false", "true"};
constexpr std::array<std::string_view, 6> toml_specials{"+inf", "+nan", "-inf",
                                                        "-nan", "inf",  "nan"};
static_assert(std::ranges::is_sorted(toml_constants));
static_assert(std::ranges::is_sorted(toml_specials));

constexpr std::string_view toml_operators = "=,[]{}.";
constexpr std::size_t triple = 3U;
// `"""` の閉じに続けてよい引用符の数（中身の終わりの `"` `""`）。
constexpr std::size_t trailing_quotes = 2U;

// 裸のキーの文字（`A-Za-z0-9_-`）。
[[nodiscard]] constexpr bool is_bare(char value) noexcept
{
    return lex_is_word(value) || value == '-';
}

[[nodiscard]] constexpr std::size_t bare_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && is_bare(line[at]))
    {
        ++at;
    }
    return at;
}

[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// `'…'`（リテラル・エスケープなし）と `"…"`（`\` で次の 1 バイトを飛ばす）の閉じの直後。
[[nodiscard]] constexpr std::size_t quote_close(std::string_view line, std::size_t open) noexcept
{
    if (line[open] == '"')
    {
        return lex_closing_quote(line, open);
    }
    const std::size_t close = line.find('\'', open + 1U);
    return close == std::string_view::npos ? lex_unclosed : close + 1U;
}

// ---- キー ----

// at のキーの 1 つ（裸か引用符）の後ろ。キーでなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t key_part_end(std::string_view line, std::size_t at) noexcept
{
    if (at >= line.size())
    {
        return lex_unclosed;
    }
    if (line[at] == '"' || line[at] == '\'')
    {
        return quote_close(line, at);
    }
    const std::size_t end = bare_end(line, at);
    return end == at ? lex_unclosed : end;
}

// at から、キー（`.` でつないだ 1 つ以上の部分）の後ろに `=` が続けば、その `=` の位置。
[[nodiscard]] constexpr std::size_t key_equals(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = key_part_end(line, at);
    while (end != lex_unclosed)
    {
        const std::size_t next = skip_spaces(line, end);
        if (next < line.size() && line[next] == '=')
        {
            return next;
        }
        const bool dotted = next < line.size() && line[next] == '.';
        end = dotted ? key_part_end(line, skip_spaces(line, next + 1U)) : lex_unclosed;
    }
    return lex_unclosed;
}

// at の前が（空白を飛ばして）行頭か `{` `,` か（キーの位置）。
[[nodiscard]] constexpr bool at_key_position(std::string_view line, std::size_t at) noexcept
{
    while (at > 0U && lex_is_space(line[at - 1U]))
    {
        --at;
    }
    return at == 0U || line[at - 1U] == '{' || line[at - 1U] == ',';
}

// at がキーの位置にあり後ろに `=` が続けば、部分を type・`.` と `=` を operators で塗って `=` の
// 後ろを返す。キーでなければ lex_unclosed。
std::size_t lex_key(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t equals = at_key_position(line, at) ? key_equals(line, at) : lex_unclosed;
    if (equals == lex_unclosed)
    {
        return lex_unclosed;
    }
    for (at = skip_spaces(line, at); at < equals; at = skip_spaces(line, at))
    {
        const bool dot = line[at] == '.';
        const std::size_t part = dot ? at + 1U : key_part_end(line, at);
        cursor.mark(at, part, dot ? SyntaxRole::operators : SyntaxRole::type);
        at = part;
    }
    cursor.mark(equals, equals + 1U, SyntaxRole::operators);
    return equals + 1U;
}

// ---- 文字列 ----

// from から 3 連引用符 delimiter の閉じの直後（後ろに続く引用符 2 つまでを含む）。`"""` の中は
// `\` で次の 1 バイトを飛ばす。閉じなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t quotes_end(std::string_view line, std::size_t at,
                                               char quote) noexcept
{
    std::size_t end = at + triple;
    while (end < line.size() && line[end] == quote && end - at < triple + trailing_quotes)
    {
        ++end;
    }
    return end;
}

[[nodiscard]] constexpr std::size_t triple_close(std::string_view line, std::size_t from,
                                                 std::string_view delimiter) noexcept
{
    const bool escapes = delimiter.front() == '"';
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line.substr(at).starts_with(delimiter))
        {
            return quotes_end(line, at, delimiter.front());
        }
        at += escapes && line[at] == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

// begin から、from の後ろの delimiter の閉じまでが string。閉じなければ次の行へ raw_string。
std::size_t mark_triple(LexCursor &cursor, std::size_t begin, std::size_t from,
                        std::string_view delimiter)
{
    const std::string_view line = cursor.line();
    const std::size_t end = triple_close(line, from, delimiter);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, delimiter));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

[[nodiscard]] constexpr bool opens_triple(std::string_view line, std::size_t open) noexcept
{
    return open + 2U < line.size() && line[open + 1U] == line[open] &&
           line[open + 2U] == line[open];
}

std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    const std::size_t key = lex_key(cursor, at);
    if (key != lex_unclosed)
    {
        return key;
    }
    const std::string_view line = cursor.line();
    if (opens_triple(line, at))
    {
        return mark_triple(cursor, at, at + triple, line.substr(at, triple));
    }
    const std::size_t end = quote_close(line, at);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::error);
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

// ---- 数と名前 ----

[[nodiscard]] constexpr bool continues_number(char value) noexcept
{
    return lex_is_word(value) || value == '.' || value == ':' || value == '+' || value == '-';
}

[[nodiscard]] constexpr std::size_t number_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && continues_number(line[at]))
    {
        ++at;
    }
    return at;
}

// 日付 `YYYY-MM-DD` の後ろに空白 1 つと時刻 `HH:` が続くか（空白で区切った日時）。
[[nodiscard]] constexpr bool spaced_time(std::string_view line, std::size_t at,
                                         std::size_t end) noexcept
{
    const std::string_view date = line.substr(at, end - at);
    const bool dated = date.size() == 10U && date[4] == '-' && date[7] == '-';
    const std::string_view hours = line.substr(end, 4U);
    return dated && hours.size() == 4U && hours[0] == ' ' && lex_is_digit(hours[1]) &&
           lex_is_digit(hours[2]) && hours[3] == ':';
}

// 数と日時は number（検証しない）。
std::size_t mark_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = number_end(line, at);
    if (spaced_time(line, at, end))
    {
        end = number_end(line, end + 1U);
    }
    cursor.mark(at, end, SyntaxRole::number);
    return end;
}

std::size_t lex_digit(LexCursor &cursor, std::size_t at)
{
    const std::size_t key = lex_key(cursor, at);
    return key != lex_unclosed ? key : mark_number(cursor, at);
}

// 値の名前: `true false` は constant、`inf nan` は number、ほかは区間なし。
std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::size_t key = lex_key(cursor, at);
    if (key != lex_unclosed)
    {
        return key;
    }
    const std::string_view line = cursor.line();
    const std::size_t end = bare_end(line, at);
    const std::string_view word = line.substr(at, end - at);
    if (std::ranges::binary_search(toml_constants, word))
    {
        cursor.mark(at, end, SyntaxRole::constant);
    }
    else if (std::ranges::binary_search(toml_specials, word))
    {
        cursor.mark(at, end, SyntaxRole::number);
    }
    return end;
}

// `+` `-` は、キー（`-` で始まる裸のキー）でなく後ろが数字か `inf` `nan` なら数。
std::size_t lex_sign(LexCursor &cursor, std::size_t at)
{
    const std::size_t key = lex_key(cursor, at);
    if (key != lex_unclosed)
    {
        return key;
    }
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    const bool special = std::ranges::binary_search(toml_specials, line.substr(at, 4U));
    return lex_is_digit(next) || special ? mark_number(cursor, at) : at + 1U;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && toml_operators.contains(line[end]))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// ---- テーブルの見出し ----

// from から見出しの閉じの `]` の位置。中身がキーの文字・引用符・`.`・空白でなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t header_close(std::string_view line, std::size_t from) noexcept
{
    std::size_t at = from;
    while (at < line.size() && line[at] != ']')
    {
        const bool plain = is_bare(line[at]) || line[at] == '.' || lex_is_space(line[at]);
        const bool quoted = line[at] == '"' || line[at] == '\'';
        at = plain ? at + 1U : (quoted ? quote_close(line, at) : lex_unclosed);
    }
    return at < line.size() ? at : lex_unclosed;
}

// 行頭の `[table]` `[[array]]` の後ろ。見出しでなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t header_end(std::string_view line, std::size_t at) noexcept
{
    const bool array = lex_next_of(line, at) == '[';
    const std::size_t close = header_close(line, at + (array ? 2U : 1U));
    if (close == lex_unclosed || (array && lex_next_of(line, close) != ']'))
    {
        return lex_unclosed;
    }
    const std::size_t end = close + (array ? 2U : 1U);
    const std::size_t rest = skip_spaces(line, end);
    return rest == line.size() || line[rest] == '#' ? end : lex_unclosed;
}

std::size_t lex_bracket(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t head = at;
    while (head > 0U && lex_is_space(line[head - 1U]))
    {
        --head;
    }
    // 前の空白だけを戻って行頭に着くか（行頭から数え直さない・1 行の走査を O(n) に保つ）。
    const std::size_t end = head == 0U ? header_end(line, at) : lex_unclosed;
    if (end == lex_unclosed)
    {
        return lex_operators(cursor, at);
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

std::size_t lex_comment(LexCursor &cursor, std::size_t at)
{
    const std::size_t end = cursor.line().size();
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 6> marks{{{'"', lex_string},
                                                                {'\'', lex_string},
                                                                {'#', lex_comment},
                                                                {'[', lex_bracket},
                                                                {'+', lex_sign},
                                                                {'-', lex_sign}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return toml_operators.contains(value) ? lex_operators : skip_character;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_digit;
    }
    return lex_is_word(value) ? lex_word : marked_handler(value);
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

// 3 連引用符の中で始まる行は、閉じまでを読んでから続ける。ほかの状態は TOML の状態でないので
// 行頭から読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    const std::string_view delimiter = lex_delimiter(in);
    switch (in.kind)
    {
    case LexKind::plain:
    case LexKind::block_comment:
        return 0U;
    case LexKind::raw_string:
        return delimiter == "\"\"\"" || delimiter == "'''" ? mark_triple(cursor, 0U, 0U, delimiter)
                                                           : 0U;
    }
    std::unreachable();
}
} // namespace

void lex_toml(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
