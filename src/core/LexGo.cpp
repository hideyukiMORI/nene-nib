#include "LexGo.hpp"

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
// 字句の関数。読み始めの位置と、行の頭のメソッドの宣言 `func (r *T) Name(` の名前の位置
// （無ければ lex_unclosed）を受けて、読み終えた次の位置を返す（必ず進む）。名前の位置は行ごとに
// 1 回だけ決めて渡す（1 行を O(n) に保つ）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。予約語は Go の仕様の 25 語
// から、Vim の go.vim が型に塗る chan と map を除いたもの。
constexpr std::array<std::string_view, 23> go_keywords{
    "break", "case",   "const",  "continue", "default", "defer",  "else",      "fallthrough",
    "for",   "func",   "go",     "goto",     "if",      "import", "interface", "package",
    "range", "return", "select", "struct",   "switch",  "type",   "var"};
// 組み込みの型（Vim の goType・goSignedInts・goUnsignedInts・goFloats・goComplexes）。
constexpr std::array<std::string_view, 24> go_types{
    "any",     "bool",    "byte", "chan",   "comparable", "complex128", "complex64", "error",
    "float32", "float64", "int",  "int16",  "int32",      "int64",      "int8",      "map",
    "rune",    "string",  "uint", "uint16", "uint32",     "uint64",     "uint8",     "uintptr"};
constexpr std::array<std::string_view, 4> go_constants{"false", "iota", "nil", "true"};
// 組み込みの関数（Vim の goBuiltins・呼び出しでなくても塗る）。
constexpr std::array<std::string_view, 18> go_builtins{
    "append", "cap", "clear", "close", "complex", "copy",  "delete",  "imag", "len",
    "make",   "max", "min",   "new",   "panic",   "print", "println", "real", "recover"};
static_assert(std::ranges::is_sorted(go_keywords));
static_assert(std::ranges::is_sorted(go_types));
static_assert(std::ranges::is_sorted(go_constants));
static_assert(std::ranges::is_sorted(go_builtins));

constexpr std::string_view operator_characters = "+-*%=<>!&|^~:.,;()[]{}";
constexpr std::string_view backquote_mark = "`";

[[nodiscard]] constexpr bool starts_name(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

// at から空白を飛ばした位置。
[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// ---- 文字列 ----

// `"…"` と rune `'…'`。閉じなければ行末まで error（Go の通常の文字列と rune は行をまたがない）。
std::size_t lex_quoted(LexCursor &cursor, std::size_t at, std::size_t /*named*/)
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

// 生の文字列 `` `…` ``（`\` で飛ばさない）。閉じなければ行末まで string で次の行へ。
std::size_t lex_raw(LexCursor &cursor, std::size_t at, std::size_t /*named*/)
{
    const std::string_view line = cursor.line();
    const std::size_t close = line.find('`', at + 1U);
    if (close == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, backquote_mark));
        return line.size();
    }
    cursor.mark(at, close + 1U, SyntaxRole::string);
    return close + 1U;
}

// ---- コメント ----

// from から `*/` を探し、`*` の位置を返す。
[[nodiscard]] constexpr std::size_t comment_close(std::string_view line, std::size_t from) noexcept
{
    for (std::size_t at = line.find('*', from); at != lex_unclosed; at = line.find('*', at + 1U))
    {
        if (lex_next_of(line, at) == '/')
        {
            return at;
        }
    }
    return lex_unclosed;
}

constexpr LexState in_block_comment{
    .kind = LexKind::block_comment, .delimiter = {}, .delimiter_length = 0U};

// `/* … */` を from から塗る（at はコメントの始まり）。閉じなければ行末まで comment で次の行へ。
std::size_t mark_block_comment(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t close = comment_close(line, from);
    if (close == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        cursor.leave(in_block_comment);
        return line.size();
    }
    cursor.mark(at, close + 2U, SyntaxRole::comment);
    return close + 2U;
}

// ---- 演算子と数字 ----

[[nodiscard]] constexpr bool starts_comment(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    return line[at] == '/' && (next == '/' || next == '*');
}

[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '.' && lex_is_digit(lex_next_of(line, at));
}

// 演算子の連なりを続けるか。コメントの始まりと `.5` で切る。
[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    const bool slash = line[at] == '/' && !starts_comment(line, at);
    return (slash || operator_characters.contains(line[at])) && !starts_number(line, at);
}

[[nodiscard]] constexpr bool is_hex_digit(char value) noexcept
{
    const char lower = static_cast<char>(value | '\x20');
    return lex_is_digit(value) || (lower >= 'a' && lower <= 'f');
}

// 数字の途中の 1 バイトが数字を続けるか。`_` と虚数の `i` は識別子の文字に含まれ、`.` は直後が桁
// （16 進の浮動小数点数 `0x1.8p1` なら 16 進の桁）のときだけ、符号は指数（10 進は `e`・16 進は
// `p`）の直後だけ。
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

std::size_t lex_number(LexCursor &cursor, std::size_t at, std::size_t /*named*/)
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

std::size_t lex_operators(LexCursor &cursor, std::size_t at, std::size_t named)
{
    const std::string_view line = cursor.line();
    if (starts_number(line, at))
    {
        return lex_number(cursor, at, named);
    }
    std::size_t end = at + 1U;
    while (end < line.size() && continues_operator(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

std::size_t lex_slash(LexCursor &cursor, std::size_t at, std::size_t named)
{
    const std::string_view line = cursor.line();
    if (lex_next_of(line, at) == '/')
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        return line.size();
    }
    if (lex_next_of(line, at) == '*')
    {
        return mark_block_comment(cursor, at, at + 2U);
    }
    return lex_operators(cursor, at, named);
}

// ---- 識別子 ----

// end の手前の空白を戻った位置。
[[nodiscard]] constexpr std::size_t skip_back_spaces(std::string_view line,
                                                     std::size_t end) noexcept
{
    while (end > 0U && lex_is_space(line[end - 1U]))
    {
        --end;
    }
    return end;
}

// at の識別子の前が空白をはさんだ `func` か（`func Name` の名前は function）。
[[nodiscard]] constexpr bool follows_func(std::string_view line, std::size_t at) noexcept
{
    constexpr std::string_view func = "func";
    const std::size_t end = skip_back_spaces(line, at);
    const std::size_t begin = end >= func.size() ? end - func.size() : 0U;
    return end < at && line.substr(begin, end - begin) == func &&
           (begin == 0U || !lex_is_word(line[begin - 1U]));
}

// 定数・予約語・型・組み込みの関数の表の順に引き、どれでもなく `func` の名前なら関数。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool named) noexcept
{
    if (std::ranges::binary_search(go_constants, word))
    {
        return SyntaxRole::constant;
    }
    if (std::ranges::binary_search(go_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (std::ranges::binary_search(go_types, word))
    {
        return SyntaxRole::type;
    }
    const bool builtin = std::ranges::binary_search(go_builtins, word);
    return builtin || named ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at, std::size_t named)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::optional<SyntaxRole> role =
        word_role(line.substr(at, end - at), at == named || follows_func(line, at));
    if (role.has_value())
    {
        cursor.mark(at, end, role.value());
    }
    return end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at, std::size_t /*named*/)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 4> marks{
        {{'"', lex_quoted}, {'\'', lex_quoted}, {'`', lex_raw}, {'/', lex_slash}}};
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

// 行が `func (` で始まるメソッドの宣言なら、受け手の `)` の後ろの名前の位置（Vim の goReceiver）。
// 受け手の `)` は 1 回だけ探す。そうでなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t method_name(std::string_view line, std::size_t first) noexcept
{
    const std::size_t end = lex_word_end(line, first);
    const std::size_t open = skip_spaces(line, end);
    if (line.substr(first, end - first) != "func" || open >= line.size() || line[open] != '(')
    {
        return lex_unclosed;
    }
    const std::size_t close = line.find(')', open);
    const std::size_t name = close == lex_unclosed ? lex_unclosed : skip_spaces(line, close + 1U);
    return name < line.size() && starts_name(line[name]) ? name : lex_unclosed;
}

// ---- 行をまたぐ状態の続き ----

// 生の文字列の中で始まる行。閉じの `` ` `` までが string で、閉じなければ行全体が string のまま。
// `` ` `` でない区切りは Go の状態でないので行頭から読む。
std::size_t resume_raw(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    if (lex_delimiter(in) != backquote_mark)
    {
        return 0U;
    }
    const std::size_t close = line.find('`');
    if (close == lex_unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, close + 1U, SyntaxRole::string);
    return close + 1U;
}

std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return mark_block_comment(cursor, 0U, 0U);
    case LexKind::raw_string:
        return resume_raw(cursor, in);
    }
    std::unreachable();
}
} // namespace

void lex_go(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::size_t start = resumed(cursor, in);
    const std::size_t named =
        start == 0U ? method_name(line, lex_first_visible(line)) : lex_unclosed;
    for (std::size_t at = start; at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at, named);
    }
}
} // namespace nenenib::core
