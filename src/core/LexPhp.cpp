#include "LexPhp.hpp"

#include "AsciiCase.hpp"
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
// 読み終えた位置と、PHP の中にいるか。
using LexProgress = std::pair<std::size_t, bool>;

// 表は小文字で ASCII の順に並べ、大小を畳んで binary_search で引く（決定 3）。予約語は PHP Manual
// の List of Keywords（`self` `parent` を足す）、定数は `true false null` と魔術定数、型は
// 予約語でない組み込みの型の名前（`array` `callable` は予約語）。
constexpr std::array<std::string_view, 73> php_keywords{"__halt_compiler",
                                                        "abstract",
                                                        "and",
                                                        "array",
                                                        "as",
                                                        "break",
                                                        "callable",
                                                        "case",
                                                        "catch",
                                                        "class",
                                                        "clone",
                                                        "const",
                                                        "continue",
                                                        "declare",
                                                        "default",
                                                        "die",
                                                        "do",
                                                        "echo",
                                                        "else",
                                                        "elseif",
                                                        "empty",
                                                        "enddeclare",
                                                        "endfor",
                                                        "endforeach",
                                                        "endif",
                                                        "endswitch",
                                                        "endwhile",
                                                        "enum",
                                                        "eval",
                                                        "exit",
                                                        "extends",
                                                        "final",
                                                        "finally",
                                                        "fn",
                                                        "for",
                                                        "foreach",
                                                        "function",
                                                        "global",
                                                        "goto",
                                                        "if",
                                                        "implements",
                                                        "include",
                                                        "include_once",
                                                        "instanceof",
                                                        "insteadof",
                                                        "interface",
                                                        "isset",
                                                        "list",
                                                        "match",
                                                        "namespace",
                                                        "new",
                                                        "or",
                                                        "parent",
                                                        "print",
                                                        "private",
                                                        "protected",
                                                        "public",
                                                        "readonly",
                                                        "require",
                                                        "require_once",
                                                        "return",
                                                        "self",
                                                        "static",
                                                        "switch",
                                                        "throw",
                                                        "trait",
                                                        "try",
                                                        "unset",
                                                        "use",
                                                        "var",
                                                        "while",
                                                        "xor",
                                                        "yield"};
constexpr std::array<std::string_view, 12> php_constants{
    "__class__",     "__dir__",      "__file__",  "__function__", "__line__", "__method__",
    "__namespace__", "__property__", "__trait__", "false",        "null",     "true"};
constexpr std::array<std::string_view, 9> php_types{
    "bool", "float", "int", "iterable", "mixed", "never", "object", "string", "void"};
static_assert(std::ranges::is_sorted(php_keywords));
static_assert(std::ranges::is_sorted(php_constants));
static_assert(std::ranges::is_sorted(php_types));

constexpr std::string_view operator_characters = "+-*/%=<>!&|^~?:.,;()[]{}@";
constexpr std::string_view quote_characters = "'\"`";

// PHP の中（plain の delimiter `php`）。既定の plain は `?>` の外。
constexpr std::string_view inside_mark = "php";
constexpr LexState inside_code = lex_entered(LexKind::plain, inside_mark);
constexpr LexState in_block_comment{
    .kind = LexKind::block_comment, .delimiter = {}, .delimiter_length = 0U};

[[nodiscard]] constexpr bool starts_name(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

// word が name（小文字）と大小を問わずに同じか。
[[nodiscard]] constexpr bool is_named(std::string_view word, std::string_view name) noexcept
{
    return word.size() == name.size() && ascii_prefix_ignoring_case(word, name);
}

[[nodiscard]] constexpr bool closes_tag(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '?' && lex_next_of(line, at) == '>';
}

// at が開きのタグ `<?php`（大小を問わず・後ろは空白か行末）か `<?=` の始まりなら、その終わり。
// そうでなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t tag_end(std::string_view line, std::size_t at) noexcept
{
    if (!line.substr(at).starts_with("<?"))
    {
        return lex_unclosed;
    }
    if (lex_next_of(line, at + 1U) == '=')
    {
        return at + 3U;
    }
    const std::size_t end = at + 5U;
    const bool php = is_named(line.substr(at + 2U, 3U), inside_mark);
    return php && (end == line.size() || lex_is_space(line[end])) ? end : lex_unclosed;
}

// from から開きのタグを探し、その始まりと終わりを返す。無ければ両方 lex_unclosed。
[[nodiscard]] constexpr std::pair<std::size_t, std::size_t> open_tag(std::string_view line,
                                                                     std::size_t from) noexcept
{
    for (std::size_t at = lex_find(line, from, "<?"); at != lex_unclosed;
         at = lex_find(line, at + 1U, "<?"))
    {
        const std::size_t end = tag_end(line, at);
        if (end != lex_unclosed)
        {
            return {at, end};
        }
    }
    return {lex_unclosed, lex_unclosed};
}

// 行頭の `#!`（shebang）。PHP の中でも外でも行末までのコメント（#861）。
[[nodiscard]] constexpr bool is_shebang(std::string_view line) noexcept
{
    return line.starts_with("#!");
}

// ---- 文字列 ----

// from から quote の閉じを探し、閉じの直後を返す（`\` は次の 1 バイトを飛ばす）。無ければ
// lex_unclosed。
[[nodiscard]] constexpr std::size_t quote_close(std::string_view line, std::size_t from,
                                                char quote) noexcept
{
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

// 引用符の文字列を from から塗る（at は文字列の始まり）。閉じなければ行末まで string で、
// 引用符を delimiter に持って次の行へ（PHP の文字列は改行を含める）。補間は分類しない。
std::size_t mark_quoted(LexCursor &cursor, std::size_t at, std::size_t from, char quote)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quote_close(line, from, quote);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, std::string_view{&quote, 1U}));
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

std::size_t lex_quoted(LexCursor &cursor, std::size_t at)
{
    return mark_quoted(cursor, at, at + 1U, cursor.line()[at]);
}

// at の `<<<` から始まるヒアドキュメント `<<<EOT` `<<<"EOT"` とナウドキュメント `<<<'EOT'` の
// 閉じの識別子。識別子の後ろは行末でなければならない（PHP の字句）。識別子が delimiter に
// 入りきらないときは nullopt（限界）。open は `<<<` と空白の後ろ（識別子か引用符）。
[[nodiscard]] constexpr std::optional<std::string_view> label_at(std::string_view line,
                                                                 std::size_t open) noexcept
{
    const char quote = open < line.size() ? line[open] : '\0';
    const bool quoted = quote == '"' || quote == '\'';
    const std::size_t begin = open + (quoted ? 1U : 0U);
    const std::size_t end = lex_word_end(line, begin);
    const bool closed = !quoted || (end < line.size() && line[end] == quote);
    const bool named = begin < line.size() && starts_name(line[begin]);
    const bool whole =
        end + (quoted ? 1U : 0U) == line.size() && end - begin <= raw_delimiter_limit;
    if (!named || !closed || !whole)
    {
        return std::nullopt;
    }
    return line.substr(begin, end - begin);
}

[[nodiscard]] constexpr std::optional<std::string_view> heredoc_label(std::string_view line,
                                                                      std::size_t at) noexcept
{
    if (!line.substr(at).starts_with("<<<"))
    {
        return std::nullopt;
    }
    std::size_t open = at + 3U;
    while (open < line.size() && lex_is_space(line[open]))
    {
        ++open;
    }
    return label_at(line, open);
}

// ---- コメントと属性 ----

// `//` と `#` の行コメント。`?>` か行末までで、`?>` は PHP の中を閉じる（PHP の字句）。
std::size_t lex_line_comment(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close = lex_find(line, at, "?>");
    const std::size_t end = close == lex_unclosed ? line.size() : close;
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

// `/* … */` を from から塗る（at はコメントの始まり）。閉じなければ行末まで comment で次の行へ。
std::size_t mark_block_comment(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t close = lex_find(line, from, "*/");
    if (close == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        cursor.leave(in_block_comment);
        return line.size();
    }
    cursor.mark(at, close + 2U, SyntaxRole::comment);
    return close + 2U;
}

// 属性 `#[…]`（PHP 8）。対応する `]` までが keyword で、中の引用符の文字列だけ string。閉じが
// 行に無ければ行末まで（行をまたがない・限界）。
// 属性の中の引用符の文字列（at は引用符）。手前の属性の切れ端 [piece, at) を keyword に、
// 文字列を string に塗って、文字列の直後（閉じなければ行末）を返す。
std::size_t mark_attribute_string(LexCursor &cursor, std::size_t piece, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quote_close(line, at + 1U, line[at]);
    const std::size_t stop = end == lex_unclosed ? line.size() : end;
    cursor.mark(piece, at, SyntaxRole::keyword);
    cursor.mark(at, stop, SyntaxRole::string);
    return stop;
}

// 角括弧の深さを 1 字ぶん進める。
[[nodiscard]] constexpr std::size_t bracket_depth(std::size_t depth, char value) noexcept
{
    if (value == '[')
    {
        return depth + 1U;
    }
    return value == ']' ? depth - 1U : depth;
}

std::size_t lex_attribute(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t piece = at;
    std::size_t depth = 1U;
    for (std::size_t index = at + 2U; index < line.size(); ++index)
    {
        if (quote_characters.contains(line[index]))
        {
            piece = mark_attribute_string(cursor, piece, index);
            index = piece - 1U;
            continue;
        }
        depth = bracket_depth(depth, line[index]);
        if (depth == 0U)
        {
            cursor.mark(piece, index + 1U, SyntaxRole::keyword);
            return index + 1U;
        }
    }
    cursor.mark(piece, line.size(), SyntaxRole::keyword);
    return line.size();
}

std::size_t lex_hash(LexCursor &cursor, std::size_t at)
{
    return lex_next_of(cursor.line(), at) == '[' ? lex_attribute(cursor, at)
                                                 : lex_line_comment(cursor, at);
}

// ---- 演算子と数字 ----

[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '.' && lex_is_digit(lex_next_of(line, at));
}

[[nodiscard]] constexpr bool starts_comment(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    return line[at] == '/' && (next == '/' || next == '*');
}

// 演算子の連なりを続けるか。`?>`・開きのタグ・コメントの始まり・`.5`・`<<<` で切る。
[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    const bool breaks = closes_tag(line, at) || tag_end(line, at) != lex_unclosed ||
                        starts_comment(line, at) || starts_number(line, at) ||
                        line.substr(at).starts_with("<<<");
    return operator_characters.contains(line[at]) && !breaks;
}

// 数字の途中の 1 バイトが数字を続けるか。識別子の文字（`_` `0x` `0b` `0o` と指数の `e`）、
// 直後が桁の `.`（16 進では続けない）、`e` の直後の符号。
[[nodiscard]] constexpr bool continues_number(std::string_view line, std::size_t at,
                                              bool hex) noexcept
{
    const char value = line[at];
    if (lex_is_word(value))
    {
        return true;
    }
    if (hex)
    {
        return false;
    }
    if (value == '.')
    {
        return lex_is_digit(lex_next_of(line, at));
    }
    return (value == '+' || value == '-') && (line[at - 1U] | '\x20') == 'e' &&
           lex_is_digit(lex_next_of(line, at));
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
    std::size_t end = at + 1U;
    while (end < line.size() && continues_operator(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

std::size_t lex_slash(LexCursor &cursor, std::size_t at)
{
    const char next = lex_next_of(cursor.line(), at);
    if (next == '/')
    {
        return lex_line_comment(cursor, at);
    }
    return next == '*' ? mark_block_comment(cursor, at, at + 2U) : lex_operators(cursor, at);
}

// 中の開きのタグ `<?php` `<?=` は keyword（状態は中のまま・#861）。`<<<` はヒアドキュメントの
// 開き（行末まで string で、閉じの識別子を delimiter に次の行へ）、そうでなければ演算子。
std::size_t lex_less(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t tag = tag_end(line, at);
    if (tag != lex_unclosed)
    {
        cursor.mark(at, tag, SyntaxRole::keyword);
        return tag;
    }
    const std::optional<std::string_view> label = heredoc_label(line, at);
    if (!label.has_value())
    {
        return lex_operators(cursor, at);
    }
    cursor.mark(at, line.size(), SyntaxRole::string);
    cursor.leave(lex_entered(LexKind::raw_string, label.value()));
    return line.size();
}

// ---- 識別子 ----

// `$name` の変数（区間なし・名前を予約語にしない・Vim の phpIdentifier は塗る色が無い）。
std::size_t skip_variable(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    return starts_name(lex_next_of(line, at)) ? lex_word_end(line, at + 1U) : at + 1U;
}

// at の名前の前が空白をはさんだ `function` か `fn` か（その名前は function）。`&` を飛ばす。
[[nodiscard]] constexpr bool follows_function(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = at;
    while (end > 0U && (lex_is_space(line[end - 1U]) || line[end - 1U] == '&'))
    {
        --end;
    }
    std::size_t begin = end;
    while (begin > 0U && lex_is_word(line[begin - 1U]))
    {
        --begin;
    }
    const std::string_view word = line.substr(begin, end - begin);
    return end < at && (is_named(word, "function") || is_named(word, "fn"));
}

// `->` か `::` の直後の名前か（プロパティ・メソッド・クラスの定数と静的メンバー）。`::class`
// の `class` だけはクラス名の解決の予約語なので除く（#861）。
[[nodiscard]] constexpr bool follows_member(std::string_view line, std::size_t at,
                                            std::string_view word) noexcept
{
    const std::string_view before = at >= 2U ? line.substr(at - 2U, 2U) : std::string_view{};
    return before == "->" || (before == "::" && !is_named(word, "class"));
}

// 定数・予約語・型の表の順に大小を畳んで引き、どれでもなく `function` の名前なら関数。`->` と
// `::` の直後の名前は区間なし。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view line, std::size_t at,
                                                  std::size_t end) noexcept
{
    const std::string_view word = line.substr(at, end - at);
    if (follows_member(line, at, word))
    {
        return std::nullopt;
    }
    if (lex_folded_in(php_constants, word))
    {
        return SyntaxRole::constant;
    }
    if (lex_folded_in(php_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (lex_folded_in(php_types, word))
    {
        return SyntaxRole::type;
    }
    return follows_function(line, at) ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::optional<SyntaxRole> role = word_role(line, at, end);
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
    constexpr std::array<std::pair<char, LexHandler>, 7> marks{{{'\'', lex_quoted},
                                                                {'"', lex_quoted},
                                                                {'`', lex_quoted},
                                                                {'/', lex_slash},
                                                                {'#', lex_hash},
                                                                {'<', lex_less},
                                                                {'$', skip_variable}}};
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

// PHP の中を at から読む。`?>` に当たれば keyword に塗ってその直後と false を、行末まで
// 読めば行末と true を返す。
LexProgress lex_code(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    while (at < line.size())
    {
        if (closes_tag(line, at))
        {
            cursor.mark(at, at + 2U, SyntaxRole::keyword);
            return {at + 2U, false};
        }
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
    return {at, true};
}

// ---- 行をまたぐ状態の続き ----

// ヒアドキュメントの本文の行。行頭の空白の後ろに閉じの識別子があり、後ろが識別子の文字で
// なければ閉じ（字下げした閉じ・PHP 7.3）で、その後ろから PHP の中を読む。
LexProgress resume_heredoc(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::string_view label = lex_delimiter(in);
    const std::size_t first = lex_first_visible(line);
    const std::size_t end = first + label.size();
    const bool closing =
        line.substr(first).starts_with(label) && (end == line.size() || !lex_is_word(line[end]));
    if (!closing)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return {line.size(), true};
    }
    cursor.mark(0U, end, SyntaxRole::string);
    return {end, true};
}

// 引用符の文字列かヒアドキュメントの中で始まる行。どちらの区切りでもなければ PHP の中の
// 行頭から読む。
LexProgress resume_raw(LexCursor &cursor, const LexState &in)
{
    const std::string_view delimiter = lex_delimiter(in);
    if (delimiter.size() == 1U && quote_characters.contains(delimiter.front()))
    {
        return {mark_quoted(cursor, 0U, 0U, delimiter.front()), true};
    }
    if (!delimiter.empty() && starts_name(delimiter.front()))
    {
        return resume_heredoc(cursor, in);
    }
    return {0U, true};
}

LexProgress resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return {0U, lex_delimiter(in) == inside_mark};
    case LexKind::block_comment:
        return {mark_block_comment(cursor, 0U, 0U), true};
    case LexKind::raw_string:
        return resume_raw(cursor, in);
    }
    std::unreachable();
}
// `?>` の外を at から読み、開きのタグを keyword に塗ってその直後を返す。タグが無ければ外の
// まま次の行へ渡して nullopt（外の HTML は区間なし）。
std::optional<std::size_t> entered(LexCursor &cursor, std::size_t at)
{
    const auto [begin, end] = open_tag(cursor.line(), at);
    if (begin == lex_unclosed)
    {
        cursor.leave(LexState{});
        return std::nullopt;
    }
    cursor.mark(begin, end, SyntaxRole::keyword);
    return end;
}

// PHP の中を at から読み、`?>` で閉じればその直後を返す。行末まで中にいれば、行をまたぐ字句が
// 状態を残していなければ PHP の中を次の行へ渡して nullopt。
std::optional<std::size_t> left_code(LexCursor &cursor, std::size_t at)
{
    const auto [next, open] = lex_code(cursor, at);
    if (!open)
    {
        return next;
    }
    cursor.leave(cursor.state().kind == LexKind::plain ? inside_code : cursor.state());
    return std::nullopt;
}
} // namespace

LexState php_start_state(bool from_first_line) noexcept
{
    return from_first_line ? LexState{} : inside_code;
}

void lex_php(LexCursor &cursor, const LexState &in)
{
    if (in.kind == LexKind::plain && is_shebang(cursor.line()))
    {
        cursor.mark(0U, cursor.line().size(), SyntaxRole::comment);
        cursor.leave(in);
        return;
    }
    const auto [at, inside] = resumed(cursor, in);
    std::optional<std::size_t> code = inside ? std::optional{at} : entered(cursor, at);
    while (code.has_value())
    {
        const std::optional<std::size_t> outside = left_code(cursor, code.value());
        code = outside.has_value() ? entered(cursor, outside.value()) : std::nullopt;
    }
}
} // namespace nenenib::core
