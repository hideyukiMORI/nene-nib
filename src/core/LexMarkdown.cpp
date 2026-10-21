#include "LexMarkdown.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 1 行の中で引いた「次の閉じの文字」の控え（#836 のレビュー R1）。閉じの文字（`]` `)` `>`）ごとに
// 「引いた位置・答え」の 2 つを持つ。引いた位置から答えまでにその文字は無いので、その間から引き直す
// ときは探さない。答えが lex_unclosed なら、引いた位置より後ろに閉じは無い。閉じの無い `[` `<` が
// 並んでも、1 行の走査は O(n)。
using CloseMemo = std::array<std::size_t, 6>;

// 行の中の字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t, CloseMemo &);

constexpr std::size_t fence_minimum = 3U;
constexpr std::size_t fence_indent_limit = 3U;
constexpr std::size_t heading_limit = 6U;
constexpr std::size_t ordered_digit_limit = 9U;

// 行末の空白。呼び出し元が除かなかった `\r` も空白に数える。
[[nodiscard]] constexpr bool is_blank(char value) noexcept
{
    return lex_is_space(value) || value == '\r';
}

[[nodiscard]] constexpr bool blank_from(std::string_view line, std::size_t at) noexcept
{
    return std::ranges::all_of(line.substr(std::min(at, line.size())), is_blank);
}

// at から同じ文字 mark が続く終わり。
[[nodiscard]] constexpr std::size_t run_end(std::string_view line, std::size_t at,
                                            char mark) noexcept
{
    while (at < line.size() && line[at] == mark)
    {
        ++at;
    }
    return at;
}

// 行頭の空白（空白 3 つまで）。それより深いか Tab なら lex_unclosed。
[[nodiscard]] constexpr std::size_t fence_indent(std::string_view line) noexcept
{
    const std::size_t at = run_end(line, 0U, ' ');
    const bool shallow = at <= fence_indent_limit && (at == line.size() || line[at] != '\t');
    return shallow ? at : lex_unclosed;
}

// ---- フェンス ----

// フェンスの状態か（raw_string で、区切りが同じ ` か ~ の 3〜16 文字）。ほかの言語の状態は plain。
[[nodiscard]] constexpr bool is_fence_state(const LexState &state) noexcept
{
    const std::string_view delimiter = lex_delimiter(state);
    return state.kind == LexKind::raw_string && delimiter.size() >= fence_minimum &&
           (delimiter.front() == '`' || delimiter.front() == '~') &&
           run_end(delimiter, 0U, delimiter.front()) == delimiter.size();
}

// 行がフェンスを開くなら、その区切り（同じ文字の 3〜16 文字）。開かなければ空。
// ``` の後ろの情報の文字列に ` があれば開かない（CommonMark）。
[[nodiscard]] constexpr std::string_view opened_fence(std::string_view line) noexcept
{
    const std::size_t at = fence_indent(line);
    if (at == lex_unclosed || at >= line.size() || (line[at] != '`' && line[at] != '~'))
    {
        return {};
    }
    const std::size_t end = run_end(line, at, line[at]);
    const std::size_t length = end - at;
    const bool ticked = line[at] == '`' && line.substr(end).contains('`');
    return length >= fence_minimum && length <= raw_delimiter_limit && !ticked
               ? line.substr(at, length)
               : std::string_view{};
}

// 行が delimiter のフェンスを閉じるか（同じ文字で同じ長さ以上・後ろは空白だけ）。
[[nodiscard]] constexpr bool closes_fence(std::string_view line,
                                          std::string_view delimiter) noexcept
{
    const std::size_t at = fence_indent(line);
    if (at == lex_unclosed)
    {
        return false;
    }
    const std::size_t end = run_end(line, at, delimiter.front());
    return end - at >= delimiter.size() && blank_from(line, end);
}

[[nodiscard]] LexState fence_state(std::string_view delimiter) noexcept
{
    LexState state{.kind = LexKind::raw_string,
                   .delimiter = {},
                   .delimiter_length = static_cast<std::uint8_t>(delimiter.size())};
    std::ranges::copy(delimiter, state.delimiter.begin());
    return state;
}

// フェンスの中の行は全体が string。閉じの行も string で、次の行へ plain を渡す。
void lex_fenced(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    cursor.mark(0U, line.size(), SyntaxRole::string);
    cursor.leave(closes_fence(line, lex_delimiter(in)) ? LexState{} : in);
}

// ---- 行内 ----

// 閉じの文字の控えの位置（`]` `)` `>` の順に 2 つずつ）。
[[nodiscard]] constexpr std::size_t memo_slot(char mark) noexcept
{
    if (mark == ']')
    {
        return 0U;
    }
    return mark == ')' ? 2U : 4U;
}

// from から閉じの文字 mark の次の位置。控えの範囲の中から引けば探さない。
[[nodiscard]] std::size_t next_close(std::string_view line, std::size_t from, char mark,
                                     CloseMemo &memo) noexcept
{
    const std::size_t slot = memo_slot(mark);
    if (from >= memo.at(slot) && from <= memo.at(slot + 1U))
    {
        return memo.at(slot + 1U);
    }
    const std::size_t found = line.find(mark, from);
    memo.at(slot) = from;
    memo.at(slot + 1U) = found;
    return found;
}

// `\` は次の 1 バイトを文字として飛ばす。
std::size_t lex_escape(LexCursor & /*cursor*/, std::size_t at, CloseMemo & /*memo*/)
{
    return at + 2U;
}

// at の ` の連なりと同じ長さの連なりまでが行内コードで string。閉じなければ連なりを飛ばす。
std::size_t lex_code_span(LexCursor &cursor, std::size_t at, CloseMemo & /*memo*/)
{
    const std::string_view line = cursor.line();
    const std::size_t open = run_end(line, at, '`');
    for (std::size_t from = line.find('`', open); from != std::string_view::npos;)
    {
        const std::size_t end = run_end(line, from, '`');
        if (end - from == open - at)
        {
            cursor.mark(at, end, SyntaxRole::string);
            return end;
        }
        from = line.find('`', end);
    }
    return open;
}

// from から delimiter（同じ印の 1〜2 文字）の次の出現の位置。無ければ lex_unclosed。1 文字ずつの
// 検索で引き、文字列の検索（core の外へ __std_search_1 を出す）を使わない。
[[nodiscard]] constexpr std::size_t delimiter_after(std::string_view line, std::size_t from,
                                                    std::string_view delimiter) noexcept
{
    for (std::size_t at = line.find(delimiter.front(), from); at != std::string_view::npos;
         at = line.find(delimiter.front(), at + 1U))
    {
        if (line.substr(at).starts_with(delimiter))
        {
            return at;
        }
    }
    return lex_unclosed;
}

// `*` `_`（2 つ続けば `**` `__`）から同じ印の次の出現までが強調で constant（決定 4・入れ子を
// 数えない）。印の直後が空白か、`_` の直前が識別子の文字なら強調にしない。閉じなければ印を飛ばす。
std::size_t lex_emphasis(LexCursor &cursor, std::size_t at, CloseMemo & /*memo*/)
{
    const std::string_view line = cursor.line();
    const char mark = line[at];
    const std::size_t width = lex_next_of(line, at) == mark ? 2U : 1U;
    const std::string_view delimiter = line.substr(at, width);
    const std::size_t inner = at + width;
    const bool opens = inner < line.size() && !is_blank(line[inner]) &&
                       !(mark == '_' && at > 0U && lex_is_word(line[at - 1U]));
    const std::size_t close = opens ? delimiter_after(line, inner, delimiter) : lex_unclosed;
    if (close == lex_unclosed)
    {
        return inner;
    }
    cursor.mark(at, close + width, SyntaxRole::constant);
    return close + width;
}

// open の `[` から `](` をはさんで最初の `)` まで（入れ子の `[]` は数えない）。無ければ
// lex_unclosed。
[[nodiscard]] std::size_t link_end(std::string_view line, std::size_t open,
                                   CloseMemo &memo) noexcept
{
    const std::size_t label = next_close(line, open + 1U, ']', memo);
    if (label == lex_unclosed || lex_next_of(line, label) != '(')
    {
        return lex_unclosed;
    }
    const std::size_t close = next_close(line, label + 2U, ')', memo);
    return close == lex_unclosed ? lex_unclosed : close + 1U;
}

// リンク `[文字](url)` は function。形にならなければ `[` を飛ばす。
std::size_t lex_link(LexCursor &cursor, std::size_t at, CloseMemo &memo)
{
    const std::size_t end = link_end(cursor.line(), at, memo);
    if (end == lex_unclosed)
    {
        return at + 1U;
    }
    cursor.mark(at, end, SyntaxRole::function);
    return end;
}

// 画像 `![alt](src)` は `!` から function。
std::size_t lex_bang(LexCursor &cursor, std::size_t at, CloseMemo &memo)
{
    const std::string_view line = cursor.line();
    const std::size_t end =
        lex_next_of(line, at) == '[' ? link_end(line, at + 1U, memo) : lex_unclosed;
    if (end == lex_unclosed)
    {
        return at + 1U;
    }
    cursor.mark(at, end, SyntaxRole::function);
    return end;
}

// HTML のタグ `<…>`（`<` の直後が英字か `/` か `!`）は keyword。閉じなければ `<` を飛ばす。
std::size_t lex_tag(LexCursor &cursor, std::size_t at, CloseMemo &memo)
{
    const std::string_view line = cursor.line();
    const char next = static_cast<char>(lex_next_of(line, at) | '\x20');
    const bool named = (next >= 'a' && next <= 'z') || next == '/' || next == '!';
    const std::size_t close = named ? next_close(line, at + 1U, '>', memo) : lex_unclosed;
    if (close == lex_unclosed)
    {
        return at + 1U;
    }
    cursor.mark(at, close + 1U, SyntaxRole::keyword);
    return close + 1U;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at, CloseMemo & /*memo*/)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 7> marks{{{'\\', lex_escape},
                                                                {'`', lex_code_span},
                                                                {'*', lex_emphasis},
                                                                {'_', lex_emphasis},
                                                                {'[', lex_link},
                                                                {'!', lex_bang},
                                                                {'<', lex_tag}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return skip_character;
}

// バイト → 行内の字句の関数の表（ADR 0156 の決定 6）。印でない文字は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

void lex_inline(LexCursor &cursor, std::size_t from)
{
    const std::string_view line = cursor.line();
    CloseMemo memo{};
    std::ranges::fill(memo, lex_unclosed);
    for (std::size_t at = from; at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at, memo);
    }
}

// ---- 行の始まり ----

// 見出し（`#` 〜 `######` の後ろが空白か行末）か。
[[nodiscard]] constexpr bool is_heading(std::string_view line, std::size_t at) noexcept
{
    const std::size_t end = run_end(line, at, '#');
    return end > at && end - at <= heading_limit && (end == line.size() || is_blank(line[end]));
}

// 水平線（`-` `*` `_` の同じ文字が 3 つ以上で、ほかは空白だけ）か。
[[nodiscard]] constexpr bool is_rule(std::string_view line, std::size_t at) noexcept
{
    const char mark = line[at];
    if (mark != '-' && mark != '*' && mark != '_')
    {
        return false;
    }
    const auto marks = std::ranges::count(line.substr(at), mark);
    const bool only = std::ranges::all_of(line.substr(at), [mark](char value)
                                          { return value == mark || is_blank(value); });
    return only && marks >= static_cast<std::ptrdiff_t>(fence_minimum);
}

// 箇条書きの記号（`-` `*` `+` か `1.` `1)`・後ろが空白か行末）の終わり。記号でなければ at。
[[nodiscard]] constexpr std::size_t list_marker_end(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = at;
    if (line[at] == '-' || line[at] == '*' || line[at] == '+')
    {
        end = at + 1U;
    }
    else
    {
        const std::size_t digits = std::min(lex_word_end(line, at), line.size());
        const bool numbered = digits > at && digits - at <= ordered_digit_limit &&
                              std::ranges::all_of(line.substr(at, digits - at), lex_is_digit) &&
                              digits < line.size() && (line[digits] == '.' || line[digits] == ')');
        end = numbered ? digits + 1U : at;
    }
    return end > at && (end == line.size() || is_blank(line[end])) ? end : at;
}

// 行全体の役割（見出し・引用・水平線）を塗れば真。
[[nodiscard]] bool marked_whole_line(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (is_heading(line, at))
    {
        cursor.mark(at, line.size(), SyntaxRole::keyword);
        return true;
    }
    if (line[at] == '>' || is_rule(line, at))
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        return true;
    }
    return false;
}

void lex_block(LexCursor &cursor)
{
    const std::string_view line = cursor.line();
    const std::string_view fence = opened_fence(line);
    if (!fence.empty())
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(fence_state(fence));
        return;
    }
    const std::size_t at = lex_first_visible(line);
    if (at == line.size() || marked_whole_line(cursor, at))
    {
        return;
    }
    const std::size_t marker = list_marker_end(line, at);
    cursor.mark(at, marker, SyntaxRole::operators);
    lex_inline(cursor, marker);
}
} // namespace

void lex_markdown(LexCursor &cursor, const LexState &in)
{
    if (is_fence_state(in))
    {
        lex_fenced(cursor, in);
        return;
    }
    lex_block(cursor);
}
} // namespace nenenib::core
