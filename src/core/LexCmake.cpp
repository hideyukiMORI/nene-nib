#include "LexCmake.hpp"

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

// 表は小文字で ASCII の順に並べ、大小を畳んで binary_search で引く（決定 3）。制御のコマンドは
// cmake-commands(7) の Scripting Commands の制御の 17 語（Vim の cmakeCommandConditional と
// cmakeCommandRepeat を含む）。定数は `if()` の真偽の定数のうち `Y` `N` `IGNORE` を除いたもの
// （1 文字の変数名や引数と衝突するため）。
constexpr std::array<std::string_view, 17> cmake_control_commands{
    "block",      "break",       "continue", "else",     "elseif",   "endblock",
    "endforeach", "endfunction", "endif",    "endmacro", "endwhile", "foreach",
    "function",   "if",          "macro",    "return",   "while"};
constexpr std::array<std::string_view, 7> cmake_constants{"false", "no",   "notfound", "off",
                                                          "on",    "true", "yes"};
static_assert(std::ranges::is_sorted(cmake_control_commands));
static_assert(std::ranges::is_sorted(cmake_constants));

// `*-NOTFOUND` の後ろの印（大小を問わない）。
constexpr std::string_view notfound_suffix = "-notfound";

constexpr LexState quoted = lex_entered(LexKind::raw_string, "\"");

[[nodiscard]] constexpr std::size_t skip_spaces(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// ---- 括弧のコメントと括弧の引数・引用符の引数 ----

// [at, …) を括弧の中身として from から閉じまで塗る（括弧のコメントは comment・括弧の引数は
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

// `"…"` を from から閉じまで塗る（`\` は次の 1 バイトを飛ばす）。閉じなければ行末まで塗り、
// 引用符の中のまま次の行へ。
std::size_t mark_quoted(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    for (std::size_t end = from; end < line.size(); ++end)
    {
        if (line[end] == '"')
        {
            cursor.mark(at, end + 1U, SyntaxRole::string);
            return end + 1U;
        }
        end += line[end] == '\\' ? 1U : 0U;
    }
    cursor.mark(at, line.size(), SyntaxRole::string);
    cursor.leave(quoted);
    return line.size();
}

std::size_t lex_quote(LexCursor &cursor, std::size_t at)
{
    return mark_quoted(cursor, at, at + 1U);
}

// `#` の後ろが括弧の開きなら括弧のコメント、そうでなければ行末までのコメント。
std::size_t lex_hash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::optional<std::size_t> level = lex_long_open(line, at + 1U);
    if (level.has_value())
    {
        return mark_long(cursor, at, at + 3U + level.value(),
                         lex_long_state(LexKind::block_comment, level.value()));
    }
    cursor.mark(at, line.size(), SyntaxRole::comment);
    return line.size();
}

// 引数の頭（行頭・空白・`(` の直後）の `[=*[` は括弧の引数。ほかの `[`（`foo[[x`）は区間なし。
std::size_t lex_bracket(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const bool head = at == 0U || lex_is_space(line[at - 1U]) || line[at - 1U] == '(';
    const std::optional<std::size_t> level =
        head ? lex_long_open(line, at) : std::optional<std::size_t>{};
    if (!level.has_value())
    {
        return at + 1U;
    }
    return mark_long(cursor, at, at + 2U + level.value(),
                     lex_long_state(LexKind::raw_string, level.value()));
}

// ---- 変数の参照とジェネレータ式 ----

// from から `{` と `}` の深さを数え、開きと釣り合う `}` の直後を返す（`${a_${b}}`）。閉じなければ
// 行末。
[[nodiscard]] constexpr std::size_t brace_end(std::string_view line, std::size_t from) noexcept
{
    std::size_t depth = 1U;
    for (std::size_t at = from; at < line.size(); ++at)
    {
        depth += line[at] == '{' ? 1U : 0U;
        depth -= line[at] == '}' ? 1U : 0U;
        if (depth == 0U)
        {
            return at + 1U;
        }
    }
    return line.size();
}

// from から `$<` と `>` の深さを数え、開きと釣り合う `>` の直後を返す
// （`$<$<CONFIG:Debug>:-g>`）。閉じなければ行末。
[[nodiscard]] constexpr std::size_t genex_end(std::string_view line, std::size_t from) noexcept
{
    std::size_t depth = 1U;
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line[at] == '$' && lex_next_of(line, at) == '<')
        {
            ++depth;
            ++at;
        }
        else if (line[at] == '>' && --depth == 0U)
        {
            return at + 1U;
        }
    }
    return line.size();
}

// `${VAR}` `$ENV{X}` `$CACHE{X}` `$<…>` は constant。ほかの `$` は区間なし。
std::size_t lex_dollar(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::string_view rest = line.substr(at + 1U);
    std::size_t end = at + 1U;
    if (rest.starts_with('{'))
    {
        end = brace_end(line, at + 2U);
    }
    else if (rest.starts_with('<'))
    {
        end = genex_end(line, at + 2U);
    }
    else if (rest.starts_with("ENV{") || rest.starts_with("CACHE{"))
    {
        end = brace_end(line, line.find('{', at) + 1U);
    }
    if (end > at + 1U)
    {
        cursor.mark(at, end, SyntaxRole::constant);
    }
    return end;
}

// ---- 数と語 ----

// 数 `3` `3.28` `1.2.3`。後ろに識別子の文字が続けば（`3rdparty`）語として区間なし。
std::size_t lex_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() &&
           (lex_is_digit(line[end]) || (line[end] == '.' && lex_is_digit(lex_next_of(line, end)))))
    {
        ++end;
    }
    if (end < line.size() && lex_is_word(line[end]))
    {
        return lex_word_end(line, end);
    }
    cursor.mark(at, end, SyntaxRole::number);
    return end;
}

// 語の後ろの `-NOTFOUND`（大小を問わず・後ろは識別子の文字でない）の終わり。無ければ end。
[[nodiscard]] constexpr std::size_t notfound_end(std::string_view line, std::size_t end) noexcept
{
    const std::size_t after = end + notfound_suffix.size();
    const bool suffixed = ascii_prefix_ignoring_case(line.substr(end), notfound_suffix) &&
                          !lex_is_word(lex_next_of(line, after - 1U));
    return suffixed ? after : end;
}

// 語。`ON` `OFF` などの定数と `*-NOTFOUND` は constant、ほかの語（大文字の引数の語も・Vim の
// ModeMsg に当たる役割が無い）は区間なし。
std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::size_t suffixed = notfound_end(line, end);
    if (suffixed != end || lex_folded_in(cmake_constants, line.substr(at, end - at)))
    {
        cursor.mark(at, suffixed, SyntaxRole::constant);
    }
    return suffixed;
}

// 行の最初の語（at）。後ろ（空白の後）が `(` ならコマンドで、制御のコマンドは keyword、ほかは
// function。そうでなければ lex_word と同じ。
std::size_t lex_first_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::size_t next = skip_spaces(line, end);
    if (next >= line.size() || line[next] != '(')
    {
        return lex_word(cursor, at);
    }
    const bool control = lex_folded_in(cmake_control_commands, line.substr(at, end - at));
    cursor.mark(at, end, control ? SyntaxRole::keyword : SyntaxRole::function);
    return end;
}

// `\` は次の 1 バイトを飛ばす。
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
    constexpr std::array<std::pair<char, LexHandler>, 5> marks{{{'"', lex_quote},
                                                                {'#', lex_hash},
                                                                {'[', lex_bracket},
                                                                {'$', lex_dollar},
                                                                {'\\', lex_escape}}};
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
    return lex_is_word(value) ? lex_word : skip_character;
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白・括弧・0x80 以上は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// 行をまたぐ状態の続き。CMake の状態でない delimiter なら行頭から読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return lex_long_level(lex_delimiter(in)) ? mark_long(cursor, 0U, 0U, in) : 0U;
    case LexKind::raw_string:
        if (in == quoted)
        {
            return mark_quoted(cursor, 0U, 0U);
        }
        return lex_long_level(lex_delimiter(in)) ? mark_long(cursor, 0U, 0U, in) : 0U;
    }
    std::unreachable();
}
} // namespace

void lex_cmake(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    std::size_t at = resumed(cursor, in);
    const std::size_t first = lex_first_visible(line);
    if (at == 0U && first < line.size() && lex_is_word(line[first]) && !lex_is_digit(line[first]))
    {
        at = lex_first_word(cursor, first);
    }
    while (at < line.size())
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
