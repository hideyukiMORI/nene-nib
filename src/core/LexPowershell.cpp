#include "LexPowershell.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "LexState.hpp"
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

// 表は小文字で ASCII の順に並べ、大小を畳んで binary_search で引く（決定 3）。キーワードは
// about_Reserved_Words と Vim の ps1.vim の ps1Keyword・ps1Conditional・ps1Repeat・ps1Exception
// を合わせたもの（`where` は Vim でも `{` の前だけなので外す）。
constexpr std::array<std::string_view, 42> powershell_keywords{
    "begin", "break",    "catch",  "class",    "clean",        "configuration", "continue",
    "data",  "default",  "define", "do",       "dynamicparam", "else",          "elseif",
    "end",   "enum",     "exit",   "filter",   "finally",      "for",           "foreach",
    "from",  "function", "hidden", "if",       "in",           "inlinescript",  "parallel",
    "param", "process",  "return", "sequence", "static",       "switch",        "throw",
    "trap",  "try",      "until",  "using",    "var",          "while",         "workflow"};
// `-` の後ろの演算子の名前（about_Comparison_Operators・about_Logical_Operators・
// about_Arithmetic_Operators の `-shl` `-shr`・about_Type_Operators・`-f` `-join` `-split`）。
// 比較は `i` `c` の頭の形も。`-` の後ろがこの表に無い名前は引数の名前（`-Path`）で区間なし。
constexpr std::array<std::string_view, 63> powershell_operators{
    "and",       "as",        "band",         "bnot",      "bor",          "bxor",
    "ccontains", "ceq",       "cge",          "cgt",       "cin",          "cle",
    "clike",     "clt",       "cmatch",       "cne",       "cnotcontains", "cnotin",
    "cnotlike",  "cnotmatch", "contains",     "creplace",  "csplit",       "eq",
    "f",         "ge",        "gt",           "icontains", "ieq",          "ige",
    "igt",       "iin",       "ile",          "ilike",     "ilt",          "imatch",
    "in",        "ine",       "inotcontains", "inotin",    "inotlike",     "inotmatch",
    "ireplace",  "is",        "isnot",        "isplit",    "join",         "le",
    "like",      "lt",        "match",        "ne",        "not",          "notcontains",
    "notin",     "notlike",   "notmatch",     "or",        "replace",      "shl",
    "shr",       "split",     "xor"};
static_assert(std::ranges::is_sorted(powershell_keywords));
static_assert(std::ranges::is_sorted(powershell_operators));

constexpr std::string_view operator_characters = "+-*/%=!<>|&,;(){}]:.?";
// `#` がコメントを始める直前のバイト（字句の頭・`a#b` の `#` はコメントでない）。
constexpr std::string_view comment_heads = ";|&(){},=";

constexpr LexState block_comment = lex_entered(LexKind::block_comment, "");
constexpr LexState single_quoted = lex_entered(LexKind::raw_string, "'");
constexpr LexState double_quoted = lex_entered(LexKind::raw_string, "\"");
constexpr LexState here_single = lex_entered(LexKind::raw_string, "@'");
constexpr LexState here_double = lex_entered(LexKind::raw_string, "@\"");

[[nodiscard]] constexpr bool is_letter(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// ---- コメントと文字列 ----

// from から `#>` を探し、[at, …) をブロックコメントとして塗る。閉じなければ行末まで塗って
// 中のまま次の行へ。
std::size_t mark_block(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t close = lex_find(line, from, "#>");
    if (close == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        cursor.leave(block_comment);
        return line.size();
    }
    cursor.mark(at, close + 2U, SyntaxRole::comment);
    return close + 2U;
}

// `$(` の後ろ from から括弧の数を数え、対応する `)` の直後を返す（中の引用符は見ない）。同じ行で
// 閉じなければ lex_unclosed（入れ子の深さは次の行へ持たない）。
[[nodiscard]] constexpr std::size_t subexpression_end(std::string_view line,
                                                      std::size_t from) noexcept
{
    std::size_t depth = 1U;
    for (std::size_t at = from; at < line.size(); ++at)
    {
        depth += line[at] == '(' ? 1U : 0U;
        depth -= line[at] == ')' ? 1U : 0U;
        if (depth == 0U)
        {
            return at + 1U;
        }
    }
    return lex_unclosed;
}

// 文字列の中の at の 1 つぶんの直後。引用符 2 つの続き（`''` `""`）と `"…"` の中の `` ` `` は
// 2 バイト（`'…'` の `` ` `` は 1 バイト）、`"…"` の中の `$(…)` は対応する `)` の直後（中の `"`
// で閉じない・同じ行で閉じなければ lex_unclosed）。
[[nodiscard]] constexpr std::size_t quoted_skip(std::string_view line, std::size_t at,
                                                char quote) noexcept
{
    const char value = line[at];
    if (quote == '"' && value == '$' && lex_next_of(line, at) == '(')
    {
        return subexpression_end(line, at + 2U);
    }
    const bool paired = value == quote || (value == '`' && quote == '"');
    return at + (paired ? 2U : 1U);
}

// from から quote の閉じを探し、閉じの直後を返す（飛ばし方は quoted_skip）。閉じなければ
// lex_unclosed。
[[nodiscard]] constexpr std::size_t quote_close(std::string_view line, std::size_t from,
                                                char quote) noexcept
{
    for (std::size_t at = from; at < line.size(); at = quoted_skip(line, at, quote))
    {
        if (line[at] == quote && lex_next_of(line, at) != quote)
        {
            return at + 1U;
        }
    }
    return lex_unclosed;
}

// [at, …) を文字列として from から閉じまで塗る。閉じなければ行末まで塗り、inside で次の行へ。
std::size_t mark_quoted(LexCursor &cursor, std::size_t at, std::size_t from, const LexState &inside)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quote_close(line, from, inside.delimiter.front());
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::string);
        cursor.leave(inside);
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

std::size_t lex_quote(LexCursor &cursor, std::size_t at)
{
    const bool single = cursor.line()[at] == '\'';
    return mark_quoted(cursor, at, at + 1U, single ? single_quoted : double_quoted);
}

// `#` は字句の頭（行頭か空白・comment_heads の直後）なら行末までコメント。
std::size_t lex_hash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char before = at > 0U ? line[at - 1U] : ' ';
    if (!lex_is_space(before) && !comment_heads.contains(before))
    {
        return at + 1U;
    }
    cursor.mark(at, line.size(), SyntaxRole::comment);
    return line.size();
}

// `@'` `@"` が行末（後ろは空白だけ）ならヒア文字列の開き。`@name` は splatting で constant。
// ほかの `@`（`@(` `@{`）は operators。
std::size_t lex_at(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    if ((next == '\'' || next == '"') && skip_spaces(line, at + 2U) == line.size())
    {
        cursor.mark(at, line.size(), SyntaxRole::string);
        cursor.leave(next == '"' ? here_double : here_single);
        return line.size();
    }
    if (is_letter(next))
    {
        const std::size_t end = lex_word_end(line, at + 1U);
        cursor.mark(at, end, SyntaxRole::constant);
        return end;
    }
    cursor.mark(at, at + 1U, SyntaxRole::operators);
    return at + 1U;
}

// ---- 変数と型 ----

// `$name` `$env:NAME` `${…}` `$_` `$?` `$$` `$^` は constant（`$true` `$false` `$null` も）。
// `$(` の `$` は operators で、中は分類を続ける。
std::size_t lex_variable(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    std::size_t end = at + 1U;
    if (next == '{')
    {
        const std::size_t close = line.find('}', at + 2U);
        end = close == lex_unclosed ? line.size() : close + 1U;
    }
    else if (lex_is_word(next))
    {
        end = lex_word_end(line, at + 1U);
        const bool scoped =
            end < line.size() && line[end] == ':' && lex_is_word(lex_next_of(line, end));
        end = scoped ? lex_word_end(line, end + 1U) : end;
    }
    else if (next == '?' || next == '$' || next == '^')
    {
        end = at + 2U;
    }
    cursor.mark(at, end, end == at + 1U ? SyntaxRole::operators : SyntaxRole::constant);
    return end;
}

// `[` の直後が名前の頭なら型 `[int]` `[System.IO.File]`（名前と `.` の続きが type）。`[` は
// operators。
std::size_t lex_bracket(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    cursor.mark(at, at + 1U, SyntaxRole::operators);
    if (!is_letter(lex_next_of(line, at)))
    {
        return at + 1U;
    }
    std::size_t end = at + 1U;
    while (end < line.size() && (lex_is_word(line[end]) || line[end] == '.'))
    {
        ++end;
    }
    cursor.mark(at + 1U, end, SyntaxRole::type);
    return end;
}

// ---- 数と演算子 ----

// `.5` の数の始まり。`..` の 2 つ目の `.` は範囲の演算子の続きで、数を始めない（`1..10`）。
[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    const bool joined = at > 0U && line[at - 1U] == '.';
    return line[at] == '.' && !joined && lex_is_digit(lex_next_of(line, at));
}

// 演算子の連なりを止める所（名前の付いた演算子と引数の名前・ブロックコメント・`.5`）。
[[nodiscard]] constexpr bool breaks_operator(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    return (line[at] == '-' && is_letter(next)) || (line[at] == '<' && next == '#') ||
           starts_number(line, at);
}

// 数字の途中の 1 バイトが数字を続けるか。識別子の文字（`0x1F` `1kb` `10d`）、`.` は直後が桁の
// ときだけ（`1..10` で切る）、符号は 10 進の指数 `e` の直後だけ。
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
        return !hex && lex_is_digit(lex_next_of(line, at));
    }
    const char exponent = static_cast<char>(line[at - 1U] | '\x20');
    return (value == '+' || value == '-') && !hex && exponent == 'e';
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

// `-` の後ろの名前。演算子の表にあれば `-` ごと operators、なければ引数の名前で区間なし。
std::size_t lex_dash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at + 1U);
    if (lex_folded_in(powershell_operators, line.substr(at + 1U, end - at - 1U)))
    {
        cursor.mark(at, end, SyntaxRole::operators);
    }
    return end;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (starts_number(line, at))
    {
        return lex_number(cursor, at);
    }
    if (line[at] == '-' && is_letter(lex_next_of(line, at)))
    {
        return lex_dash(cursor, at);
    }
    if (line[at] == '<' && lex_next_of(line, at) == '#')
    {
        return mark_block(cursor, at, at + 2U);
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

// ---- 名前 ----

// `function` などの後ろの名前（`global:Get-Thing` のようにスコープと `-` を含む）を role で塗る。
std::size_t mark_name(LexCursor &cursor, std::size_t from, SyntaxRole role)
{
    const std::string_view line = cursor.line();
    const std::size_t at = skip_spaces(line, from);
    std::size_t end = at;
    while (end < line.size() && (lex_is_word(line[end]) || line[end] == '-' || line[end] == ':'))
    {
        ++end;
    }
    cursor.mark(at, end, role);
    return end;
}

// 名前の後ろの読み方。`function` `filter` `workflow` `configuration` の後ろは function、`class`
// `enum` の後ろは type。
std::size_t after_keyword(LexCursor &cursor, std::size_t end, std::string_view word)
{
    const bool callable = lex_folded_equal(word, "function") || lex_folded_equal(word, "filter") ||
                          lex_folded_equal(word, "workflow") ||
                          lex_folded_equal(word, "configuration");
    if (callable)
    {
        return mark_name(cursor, end, SyntaxRole::function);
    }
    const bool typed = lex_folded_equal(word, "class") || lex_folded_equal(word, "enum");
    return typed ? mark_name(cursor, end, SyntaxRole::type) : end;
}

// 語。`Verb-Noun` の形（`-` と語が続く）は function。`.` `:` の後ろ（メンバー）でなければ、
// キーワードの表（大小を問わない）にある語は keyword。
std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = lex_word_end(line, at);
    if (end < line.size() && line[end] == '-' && lex_is_word(lex_next_of(line, end)))
    {
        while (end < line.size() && line[end] == '-' && lex_is_word(lex_next_of(line, end)))
        {
            end = lex_word_end(line, end + 1U);
        }
        cursor.mark(at, end, SyntaxRole::function);
        return end;
    }
    const std::string_view word = line.substr(at, end - at);
    const bool member = at > 0U && (line[at - 1U] == '.' || line[at - 1U] == ':');
    if (member || !lex_folded_in(powershell_keywords, word))
    {
        return end;
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return after_keyword(cursor, end, word);
}

// `` ` `` は次の 1 バイトを飛ばす（行末なら行の続き）。
std::size_t lex_escape(LexCursor &cursor, std::size_t at)
{
    return std::min(at + 2U, cursor.line().size());
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 7> marks{{{'\'', lex_quote},
                                                                {'"', lex_quote},
                                                                {'$', lex_variable},
                                                                {'@', lex_at},
                                                                {'#', lex_hash},
                                                                {'[', lex_bracket},
                                                                {'`', lex_escape}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    if (lex_is_word(value))
    {
        return lex_word;
    }
    return operator_characters.contains(value) ? lex_operators : skip_character;
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

// ---- 行をまたぐ状態の続き ----

// ヒア文字列の中で始まる行。行頭の `"@`（`@'` の中は `'@`）で閉じて残りを読む。ほかの行は全体が
// string（字下げした閉じは閉じない・about_Quoting_Rules）。
std::size_t resume_here(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::string_view closing = in == here_double ? "\"@" : "'@";
    if (line.starts_with(closing))
    {
        cursor.mark(0U, 2U, SyntaxRole::string);
        return 2U;
    }
    cursor.mark(0U, line.size(), SyntaxRole::string);
    cursor.leave(in);
    return line.size();
}

// 文字列の中で始まる行。PowerShell の状態でない delimiter なら行頭から読む。
std::size_t resume_string(LexCursor &cursor, const LexState &in)
{
    if (in == single_quoted || in == double_quoted)
    {
        return mark_quoted(cursor, 0U, 0U, in);
    }
    if (in == here_single || in == here_double)
    {
        return resume_here(cursor, in);
    }
    return 0U;
}

std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return in == block_comment ? mark_block(cursor, 0U, 0U) : 0U;
    case LexKind::raw_string:
        return resume_string(cursor, in);
    }
    std::unreachable();
}
} // namespace

void lex_powershell(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
