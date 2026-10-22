#include "LexShell.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "LexState.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。shell は大小を区別する。
// キーワードは Bash Reference Manual の Reserved Words（`!` `{` `}` `[[` `]]` は記号の語として
// 別に見る）。組み込みは Vim の sh.vim の shStatement（bash の既定）と `echo` `set` `unset`
// `alias`。
constexpr std::array<std::string_view, 16> shell_keywords{
    "case",     "do", "done", "elif",   "else", "esac", "fi",    "for",
    "function", "if", "in",   "select", "then", "time", "until", "while"};
constexpr std::array<std::string_view, 58> shell_builtins{
    "alias",    "bg",       "bind",    "break",    "builtin", "caller", "cd",      "command",
    "compgen",  "complete", "compopt", "continue", "declare", "dirs",   "disown",  "echo",
    "enable",   "eval",     "exec",    "exit",     "export",  "false",  "fc",      "fg",
    "getopts",  "hash",     "help",    "history",  "jobs",    "kill",   "let",     "local",
    "logout",   "mapfile",  "popd",    "printf",   "pushd",   "pwd",    "read",    "readarray",
    "readonly", "return",   "set",     "shift",    "shopt",   "source", "suspend", "test",
    "times",    "trap",     "true",    "type",     "typeset", "ulimit", "umask",   "unalias",
    "unset",    "wait"};
static_assert(std::ranges::is_sorted(shell_keywords));
static_assert(std::ranges::is_sorted(shell_builtins));

// 語を切るバイト（Bash の metacharacter と引用符・展開・`\`）。`#` は語の途中では語の一部。
constexpr std::string_view word_stops = " \t;|&()<>'\"`$\\";
// 続けて 1 つの operators にする演算子（`<` と `(` は呼び手が見る）。
constexpr std::string_view operator_characters = ";|&)>";
// `#` がコメントを始める直前のバイト（語の頭・決定 4）。
constexpr std::string_view comment_heads = ";|&(";
// これだけでできた語は operators（`{` `}` `!` `[` `]` `==` `=~` `!=`）。
constexpr std::string_view symbol_characters = "{}[]!=~";
// ヒアドキュメントの閉じの語を切るバイト。
constexpr std::string_view heredoc_stops = " \t|&;()<>'\"";
// 閉じの語の長さの上限（delimiter の頭の印の 1 バイトを除く）。
constexpr std::size_t heredoc_word_limit = raw_delimiter_limit - 1U;

constexpr LexState single_quoted = lex_entered(LexKind::raw_string, "'");
constexpr LexState double_quoted = lex_entered(LexKind::raw_string, "\"");
constexpr LexState ansi_quoted = lex_entered(LexKind::raw_string, "$'");

// ヒアドキュメントの印の終わりと、本文の行へ渡す状態。
using Heredoc = std::pair<std::size_t, LexState>;

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

// ヒアドキュメントの中の状態か（delimiter の頭が `<` か `-`）。
[[nodiscard]] constexpr bool in_heredoc(const LexState &state) noexcept
{
    const std::string_view delimiter = lex_delimiter(state);
    return state.kind == LexKind::raw_string &&
           (delimiter.starts_with('<') || delimiter.starts_with('-'));
}

// ---- 文字列 ----

// from から inside の引用符の閉じを探し、閉じの直後を返す。`'…'` は `\` で飛ばさず、`"…"` と
// `$'…'` は `\` が次の 1 バイトを飛ばす。閉じなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t quote_close(std::string_view line, std::size_t from,
                                                const LexState &inside) noexcept
{
    if (inside == single_quoted)
    {
        const std::size_t close = line.find('\'', from);
        return close == lex_unclosed ? lex_unclosed : close + 1U;
    }
    const char quote = lex_delimiter(inside).back();
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line[at] == quote)
        {
            return at + 1U;
        }
        at += line[at] == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

// [at, …) を文字列として from から閉じまで塗る。閉じなければ行末まで塗り、inside で次の行へ。
std::size_t mark_quoted(LexCursor &cursor, std::size_t at, std::size_t from, const LexState &inside)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quote_close(line, from, inside);
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

// ---- ヒアドキュメント ----

// from から始まる閉じの語（`'…'` `"…"` で囲むか `\` を頭に付けてもよい）の始まり・終わりと、
// 印の終わり。語が無い・heredoc_word_limit を越える・引用符が閉じないときは nullopt。探すのは
// 上限の長さまでで、1 つの印の費用は定数。
[[nodiscard]] constexpr std::optional<std::array<std::size_t, 3>>
quoted_heredoc_word(std::string_view line, std::size_t from) noexcept
{
    const std::size_t close = line.substr(from + 1U, heredoc_word_limit + 1U).find(line[from]);
    if (close == std::string_view::npos || close == 0U)
    {
        return std::nullopt;
    }
    return std::array<std::size_t, 3>{from + 1U, from + 1U + close, from + 2U + close};
}

[[nodiscard]] constexpr std::optional<std::array<std::size_t, 3>>
heredoc_word(std::string_view line, std::size_t from) noexcept
{
    const char quote = from < line.size() ? line[from] : '\0';
    if (quote == '\'' || quote == '"')
    {
        return quoted_heredoc_word(line, from);
    }
    const std::size_t begin = from + (quote == '\\' ? 1U : 0U);
    std::size_t end = begin;
    while (end < line.size() && end - begin <= heredoc_word_limit &&
           !heredoc_stops.contains(line[end]))
    {
        ++end;
    }
    if (end == begin || end - begin > heredoc_word_limit)
    {
        return std::nullopt;
    }
    return std::array<std::size_t, 3>{begin, end, end};
}

// at の `<<` から始まる印 `<<EOF` `<<-EOF` `<< 'EOF'` `<<"EOF"` `<<\EOF`。本文の行へ渡す状態の
// delimiter は頭の印（`<`・`<<-` は `-`）と閉じの語。
[[nodiscard]] constexpr std::optional<Heredoc> heredoc_marker(std::string_view line,
                                                              std::size_t at) noexcept
{
    const bool dashed = lex_next_of(line, at + 1U) == '-';
    const std::optional<std::array<std::size_t, 3>> word =
        heredoc_word(line, skip_spaces(line, at + (dashed ? 3U : 2U)));
    if (!word.has_value())
    {
        return std::nullopt;
    }
    const auto [begin, end, after] = word.value();
    LexState state = lex_entered(LexKind::raw_string, dashed ? "-" : "<");
    std::ranges::copy(line.substr(begin, end - begin), std::next(state.delimiter.begin()));
    state.delimiter_length = static_cast<std::uint8_t>(1U + end - begin);
    return Heredoc{after, state};
}

// ヒアドキュメントの本文の行。閉じの語だけの行（`<<-` は行頭のタブを除いて）で plain に戻る。
// 本文と閉じの行は全体が string（中の展開は分類しない）。
void lex_heredoc_body(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::string_view delimiter = lex_delimiter(in);
    const bool dashed = delimiter.starts_with('-');
    std::size_t first = 0U;
    while (dashed && first < line.size() && line[first] == '\t')
    {
        ++first;
    }
    cursor.mark(0U, line.size(), SyntaxRole::string);
    cursor.leave(line.substr(first) == delimiter.substr(1U) ? LexState{} : in);
}

// `<`。算術の外の `<<`（`<<<` でない）でヒアドキュメントの印なら string に塗り、最初の印なら
// 本文へ渡す状態を heredoc に覚える。ほかは `<` `>` `&` の続きを operators に。
std::size_t lex_less(LexCursor &cursor, std::size_t at, bool arithmetic,
                     std::optional<LexState> &heredoc)
{
    const std::string_view line = cursor.line();
    const bool doubled = lex_next_of(line, at) == '<' && lex_next_of(line, at + 1U) != '<';
    const std::optional<Heredoc> marker =
        doubled && !arithmetic ? heredoc_marker(line, at) : std::nullopt;
    if (marker.has_value())
    {
        cursor.mark(at, marker.value().first, SyntaxRole::string);
        heredoc = heredoc.has_value() ? heredoc : marker.value().second;
        return marker.value().first;
    }
    std::size_t end = at + 1U;
    while (end < line.size() && (line[end] == '<' || line[end] == '>' || line[end] == '&'))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// `(`。算術の外の `((` は閉じの `))` までを算術にして（閉じが無ければ行末まで）、中の `<<` を
// ヒアドキュメントにしない。探すのは算術の外の `((` だけで、1 行で O(n)。
std::size_t lex_paren(LexCursor &cursor, std::size_t at, std::size_t &arithmetic_end)
{
    const std::string_view line = cursor.line();
    if (at < arithmetic_end || lex_next_of(line, at) != '(')
    {
        cursor.mark(at, at + 1U, SyntaxRole::operators);
        return at + 1U;
    }
    const std::size_t close = lex_find(line, at + 2U, "))");
    arithmetic_end = close == lex_unclosed ? line.size() : close + 2U;
    cursor.mark(at, at + 2U, SyntaxRole::operators);
    return at + 2U;
}

// ---- 展開と記号 ----

// `$` から始まる展開の終わり。展開でなければ at + 1。
[[nodiscard]] constexpr std::size_t dollar_end(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    if (next == '{')
    {
        const std::size_t close = line.find('}', at + 2U);
        return close == lex_unclosed ? line.size() : close + 1U;
    }
    if (is_letter(next))
    {
        return lex_word_end(line, at + 1U);
    }
    const bool special = lex_is_digit(next) || std::string_view{"@*#?$!-"}.contains(next);
    return at + (special ? 2U : 1U);
}

// `$name` `${…}` `$1` `$@` `$?` などは constant。`$'…'` `$"…"` は string。`$(` の `$` は
// operators で、中は分類を続ける。
std::size_t lex_dollar(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    if (next == '\'' || next == '"')
    {
        return mark_quoted(cursor, at, at + 2U, next == '"' ? double_quoted : ansi_quoted);
    }
    const std::size_t end = dollar_end(line, at);
    cursor.mark(at, end, end == at + 1U ? SyntaxRole::operators : SyntaxRole::constant);
    return end;
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && operator_characters.contains(line[end]))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// `` ` `` のコマンド置換の印は operators（中は分類を続ける・入れ子は限界）。
std::size_t lex_backquote(LexCursor &cursor, std::size_t at)
{
    cursor.mark(at, at + 1U, SyntaxRole::operators);
    return at + 1U;
}

// `\` は次の 1 バイトを飛ばす（行末なら行の続き）。
std::size_t lex_escape(LexCursor &cursor, std::size_t at)
{
    return std::min(at + 2U, cursor.line().size());
}

// ---- 語 ----

// バイト → 語を切るか の表（word_stops を 1 回だけ引いて作る・1 バイトごとに探さない）。
constexpr auto stops = []
{
    std::array<bool, 256> table{};
    for (const char value : word_stops)
    {
        table.at(static_cast<unsigned char>(value)) = true;
    }
    return table;
}();

[[nodiscard]] constexpr std::size_t word_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && !stops.at(static_cast<unsigned char>(line[at])))
    {
        ++at;
    }
    return at;
}

// 関数の名前に使える語か（Vim の shFunctionOne の `[-a-zA-Z_0-9:]`・`.` も）。
[[nodiscard]] constexpr bool is_name(std::string_view word) noexcept
{
    return std::ranges::all_of(
        word, [](char value)
        { return lex_is_word(value) || value == '-' || value == ':' || value == '.'; });
}

// `function` の後ろの名前を function に塗る。
std::size_t mark_function_name(LexCursor &cursor, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t at = skip_spaces(line, from);
    const std::size_t end = word_end(line, at);
    if (is_name(line.substr(at, end - at)))
    {
        cursor.mark(at, end, SyntaxRole::function);
    }
    return end;
}

// 語の役割。数字だけは number、`[[` `]]` とキーワードは keyword、記号だけの語は operators、
// 組み込みと `name()` の定義の名前は function。どれでもなければ nullopt。
[[nodiscard]] constexpr std::optional<SyntaxRole> word_role(std::string_view line, std::size_t at,
                                                            std::size_t end) noexcept
{
    const std::string_view word = line.substr(at, end - at);
    if (std::ranges::all_of(word, lex_is_digit))
    {
        return SyntaxRole::number;
    }
    if (word == "[[" || word == "]]" || std::ranges::binary_search(shell_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (std::ranges::all_of(word, [](char value) { return symbol_characters.contains(value); }))
    {
        return SyntaxRole::operators;
    }
    const bool defined = is_name(word) && line.substr(skip_spaces(line, end)).starts_with("()");
    if (defined || std::ranges::binary_search(shell_builtins, word))
    {
        return SyntaxRole::function;
    }
    return std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = std::max(word_end(line, at), at + 1U);
    const std::optional<SyntaxRole> role = word_role(line, at, end);
    if (!role.has_value())
    {
        return end;
    }
    cursor.mark(at, end, role.value());
    return line.substr(at, end - at) == "function" ? mark_function_name(cursor, end) : end;
}

// `#` は語の頭（行頭か空白・comment_heads の直後）なら行末までコメント（`#!` も同じ・決定 4）。
// 語の途中（`a#b`）は語の一部。
std::size_t lex_hash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char before = at > 0U ? line[at - 1U] : ' ';
    if (!lex_is_space(before) && !comment_heads.contains(before))
    {
        return lex_word(cursor, at);
    }
    cursor.mark(at, line.size(), SyntaxRole::comment);
    return line.size();
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 9> marks{{{'\'', lex_quote},
                                                                {'"', lex_quote},
                                                                {'$', lex_dollar},
                                                                {'`', lex_backquote},
                                                                {'\\', lex_escape},
                                                                {'#', lex_hash},
                                                                {'<', skip_character},
                                                                {'(', skip_character},
                                                                {' ', skip_character}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    if (lex_is_space(value))
    {
        return skip_character;
    }
    return operator_characters.contains(value) ? lex_operators : lex_word;
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。`<` と `(` は lex_shell が先に見る。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// 文字列の中で始まる行。shell の状態でない delimiter なら行頭から読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    const bool quoted = in == single_quoted || in == double_quoted || in == ansi_quoted;
    return quoted ? mark_quoted(cursor, 0U, 0U, in) : 0U;
}
} // namespace

void lex_shell(LexCursor &cursor, const LexState &in)
{
    if (in_heredoc(in))
    {
        lex_heredoc_body(cursor, in);
        return;
    }
    const std::string_view line = cursor.line();
    std::size_t arithmetic_end = 0U;
    std::optional<LexState> heredoc;
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        const char value = line[at];
        if (value == '<')
        {
            at = lex_less(cursor, at, at < arithmetic_end, heredoc);
        }
        else if (value == '(')
        {
            at = lex_paren(cursor, at, arithmetic_end);
        }
        else
        {
            at = handlers.at(static_cast<unsigned char>(value))(cursor, at);
        }
    }
    if (heredoc.has_value())
    {
        cursor.leave(heredoc.value());
    }
}
} // namespace nenenib::core
