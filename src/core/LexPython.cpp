#include "LexPython.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。定数が予約語より先に勝つ。
constexpr std::array<std::string_view, 37> python_keywords{
    "False",  "None",     "True",   "and", "as",    "assert", "async",  "await",    "break", "case",
    "class",  "continue", "def",    "del", "elif",  "else",   "except", "finally",  "for",   "from",
    "global", "if",       "import", "in",  "is",    "lambda", "match",  "nonlocal", "not",   "or",
    "pass",   "raise",    "return", "try", "while", "with",   "yield"};
constexpr std::array<std::string_view, 5> python_constants{"Ellipsis", "False", "None",
                                                           "NotImplemented", "True"};
constexpr std::array<std::string_view, 15> python_types{
    "bool", "bytearray", "bytes", "complex", "dict", "float", "frozenset", "int",
    "list", "object",    "range", "set",     "str",  "tuple", "type"};
// 文字列の接頭辞（小文字に畳んで引く・`Rb"` `BR'` も同じ）。
constexpr std::array<std::string_view, 8> string_prefixes{"b", "br", "f",  "fr",
                                                          "r", "rb", "rf", "u"};
static_assert(std::ranges::is_sorted(python_keywords));
static_assert(std::ranges::is_sorted(python_constants));
static_assert(std::ranges::is_sorted(python_types));
static_assert(std::ranges::is_sorted(string_prefixes));

constexpr std::string_view operator_characters = "+-*/%=<>!&|^~.,:;()[]{}@";
constexpr std::size_t triple = 3U;

// ---- 文字列 ----

[[nodiscard]] constexpr bool opens_triple(std::string_view line, std::size_t open) noexcept
{
    return open + 2U < line.size() && line[open + 1U] == line[open] &&
           line[open + 2U] == line[open];
}

// from から 3 連引用符 delimiter の閉じを探し、その直後を返す。`\` は次の 1 バイトを飛ばす。
[[nodiscard]] constexpr std::size_t triple_close(std::string_view line, std::size_t from,
                                                 std::string_view delimiter) noexcept
{
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line.substr(at).starts_with(delimiter))
        {
            return at + delimiter.size();
        }
        at += line[at] == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

[[nodiscard]] LexState triple_state(std::string_view delimiter) noexcept
{
    LexState state{.kind = LexKind::raw_string,
                   .delimiter = {},
                   .delimiter_length = static_cast<std::uint8_t>(delimiter.size())};
    std::ranges::copy(delimiter, state.delimiter.begin());
    return state;
}

// begin（接頭辞の先頭）から open の 3 連引用符。閉じていなければ行末まで string で次の行へ。
std::size_t mark_triple(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::string_view delimiter = line.substr(open, triple);
    const std::size_t end = triple_close(line, open + triple, delimiter);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(triple_state(delimiter));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

// begin（接頭辞の先頭）から open の引用符で始まる文字列。1 つの引用符の文字列は行をまたがず、
// 閉じていなければ行末まで error。
std::size_t mark_string(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    if (opens_triple(line, open))
    {
        return mark_triple(cursor, begin, open);
    }
    const std::size_t end = lex_closing_quote(line, open);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::error);
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    return mark_string(cursor, at, at);
}

// ---- コメントと演算子と数字 ----

std::size_t lex_comment(LexCursor &cursor, std::size_t at)
{
    const std::size_t end = cursor.line().size();
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '.' && lex_is_digit(lex_next_of(line, at));
}

// 数字の途中の 1 バイトが数字を続けるか。`_` と `j` は識別子の文字に含まれ、符号は 16 進でない
// 数の指数の直後だけ。
[[nodiscard]] constexpr bool continues_number(std::string_view line, std::size_t at,
                                              bool hex) noexcept
{
    const char value = line[at];
    if (lex_is_word(value) || (value == '.' && !hex))
    {
        return true;
    }
    const char exponent = static_cast<char>(line[at - 1U] | '\x20');
    return !hex && (value == '+' || value == '-') && exponent == 'e';
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

// 演算子の連なりを続けるか。数字の始まりの `.`（`.5`）で切る。
[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    return operator_characters.contains(line[at]) && !starts_number(line, at);
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (starts_number(line, at))
    {
        return lex_number(cursor, at);
    }
    std::size_t end = at + 1U;
    while (end < line.size() && continues_operator(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// at の後ろの空白を飛ばした所に名前の先頭（数字でない識別子の文字）があればその位置。
[[nodiscard]] constexpr std::size_t decorator_name(std::string_view line, std::size_t at) noexcept
{
    std::size_t name = at + 1U;
    while (name < line.size() && lex_is_space(line[name]))
    {
        ++name;
    }
    const bool named = name < line.size() && lex_is_word(line[name]) && !lex_is_digit(line[name]);
    return named ? name : lex_unclosed;
}

// 行の最初の文字の `@` の後ろに名前（`.` でつないだ名前）が続けばデコレータで keyword。行の途中の
// `@` は行列の積の演算子。
std::size_t lex_at_sign(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = decorator_name(line, at);
    if (at != lex_first_visible(line) || end == lex_unclosed)
    {
        return lex_operators(cursor, at);
    }
    while (end < line.size() && (lex_is_word(line[end]) || line[end] == '.'))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

// ---- 識別子 ----

// 小文字に畳んだ word が文字列の接頭辞か（`rb` `Rb` `bR` …）。
[[nodiscard]] bool is_string_prefix(std::string_view word) noexcept
{
    std::array<char, 2> lowered{};
    if (word.empty() || word.size() > lowered.size())
    {
        return false;
    }
    std::ranges::transform(word, lowered.begin(),
                           [](char value) { return static_cast<char>(value | '\x20'); });
    return std::ranges::binary_search(string_prefixes,
                                      std::string_view{lowered.data(), word.size()});
}

// at の識別子の前が空白をはさんだ `def` か `class` か（その名前は function）。
[[nodiscard]] bool follows_definition(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = at;
    while (end > 0U && lex_is_space(line[end - 1U]))
    {
        --end;
    }
    std::size_t begin = end;
    while (begin > 0U && lex_is_word(line[begin - 1U]))
    {
        --begin;
    }
    const std::string_view word = line.substr(begin, end - begin);
    return end < at && (word == "def" || word == "class");
}

// 定数・予約語・型の表の順に引き、どれでもなく直後が `(` か def / class の名前なら関数。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool named) noexcept
{
    if (std::ranges::binary_search(python_constants, word))
    {
        return SyntaxRole::constant;
    }
    if (std::ranges::binary_search(python_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (std::ranges::binary_search(python_types, word))
    {
        return SyntaxRole::type;
    }
    return named ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::string_view word = line.substr(at, end - at);
    const bool quoted = end < line.size() && (line[end] == '"' || line[end] == '\'');
    if (quoted && is_string_prefix(word))
    {
        return mark_string(cursor, at, end);
    }
    const bool named = (end < line.size() && line[end] == '(') || follows_definition(line, at);
    const std::optional<SyntaxRole> role = word_role(word, named);
    if (role.has_value())
    {
        cursor.mark(at, end, role.value());
    }
    return end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 4> marks{
        {{'"', lex_string}, {'\'', lex_string}, {'#', lex_comment}, {'@', lex_at_sign}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return operator_characters.contains(value) ? lex_operators : skip_character;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    return lex_is_word(value) ? lex_word : marked_handler(value);
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

// 3 連引用符の中で始まる行。閉じの 3 文字までが string で、閉じなければ行全体が string のまま。
std::size_t resume_triple(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::size_t end = triple_close(line, 0U, lex_delimiter(in));
    if (end == lex_unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, end, SyntaxRole::string);
    return end;
}

// Python にブロックコメントは無く、3 文字でない区切りは Python の状態でないので行頭から読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
    case LexKind::block_comment:
        return 0U;
    case LexKind::raw_string:
        return in.delimiter_length == triple ? resume_triple(cursor, in) : 0U;
    }
    std::unreachable();
}
} // namespace

void lex_python(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
