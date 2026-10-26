#include "LexYaml.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace nenenib::core
{
namespace
{
// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。
constexpr std::array<std::string_view, 16> yaml_constants{
    "FALSE", "False", "NO",    "NULL", "No",   "Null", "TRUE", "True",
    "YES",   "Yes",   "false", "no",   "null", "true", "yes",  "~"};
constexpr std::array<std::string_view, 6> yaml_specials{".INF", ".Inf", ".NAN",
                                                        ".NaN", ".inf", ".nan"};
static_assert(std::ranges::is_sorted(yaml_constants));
static_assert(std::ranges::is_sorted(yaml_specials));

constexpr std::string_view flow_indicators = "[]{},";
// 行頭のキーにならない最初の文字（YAML の indicator）。
constexpr std::string_view key_indicators = "\"'[]{},&*!|>#%@`?";
// ブロックスカラーの字下げの深さの上限（delimiter の 2 桁）。
constexpr std::size_t indent_limit = 99U;
constexpr std::size_t decimal = 10U;

// ---- 字下げとブロックスカラー ----

[[nodiscard]] constexpr std::size_t indent_of(std::string_view line) noexcept
{
    std::size_t at = 0;
    while (at < line.size() && line[at] == ' ')
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

[[nodiscard]] LexState block_state(std::size_t indent) noexcept
{
    const std::size_t depth = std::min(indent, indent_limit);
    const std::array<char, 2> digits{static_cast<char>('0' + (depth / decimal)),
                                     static_cast<char>('0' + (depth % decimal))};
    return lex_entered(LexKind::raw_string, std::string_view{digits.data(), digits.size()});
}

// ブロックスカラーの状態の字下げの深さ。この言語の状態でなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t block_indent(const LexState &in) noexcept
{
    const std::string_view delimiter = lex_delimiter(in);
    const bool digits = in.kind == LexKind::raw_string && delimiter.size() == 2U &&
                        lex_is_digit(delimiter[0]) && lex_is_digit(delimiter[1]);
    if (!digits)
    {
        return lex_unclosed;
    }
    return (static_cast<std::size_t>(delimiter[0] - '0') * decimal) +
           static_cast<std::size_t>(delimiter[1] - '0');
}

// ブロックスカラーの中で始まる行。空行と、開いた行より深い字下げの行は全体が string で状態を
// 保つ（true）。同じかより浅い字下げの行と、ブロックスカラーでない状態は普通に読む（false）。
[[nodiscard]] bool continues_block(LexCursor &cursor, const LexState &in)
{
    const std::size_t depth = block_indent(in);
    const std::string_view line = cursor.line();
    const bool blank = skip_spaces(line, 0U) == line.size();
    if (depth == lex_unclosed || (!blank && indent_of(line) <= depth))
    {
        return false;
    }
    cursor.mark(0U, line.size(), SyntaxRole::string);
    cursor.leave(in);
    return true;
}

// ---- 数と定数 ----

[[nodiscard]] constexpr std::size_t digits_end(std::string_view text, std::size_t at) noexcept
{
    while (at < text.size() && lex_is_digit(text[at]))
    {
        ++at;
    }
    return at;
}

// at の `e` から指数（`e` `E` と符号と数字）の後ろの位置。数字が無ければ lex_unclosed。
[[nodiscard]] constexpr std::size_t exponent_end(std::string_view text, std::size_t at) noexcept
{
    std::size_t sign = at + 1U;
    if (sign < text.size() && (text[sign] == '+' || text[sign] == '-'))
    {
        ++sign;
    }
    const std::size_t end = digits_end(text, sign);
    return end == sign ? lex_unclosed : end;
}

// 符号の無い 10 進の数（`12` `1.5` `.5` `1.` `6.02e+23`）。
[[nodiscard]] constexpr bool is_decimal(std::string_view text) noexcept
{
    const std::size_t whole = digits_end(text, 0U);
    std::size_t at = whole;
    if (at < text.size() && text[at] == '.')
    {
        at = digits_end(text, at + 1U);
    }
    const bool digits = whole > 0U || at > whole + 1U;
    if (at < text.size() && (text[at] == 'e' || text[at] == 'E'))
    {
        at = exponent_end(text, at);
    }
    return digits && at == text.size();
}

// `0x` の 16 進と `0o` の 8 進。
[[nodiscard]] constexpr bool is_prefixed(std::string_view text) noexcept
{
    if (text.size() < 3U || text[0] != '0')
    {
        return false;
    }
    const std::string_view body = text.substr(2U);
    const auto hex = [](char value)
    { return lex_is_digit(value) || ((value | '\x20') >= 'a' && (value | '\x20') <= 'f'); };
    const auto octal = [](char value) { return value >= '0' && value <= '7'; };
    return (text[1] == 'x' && std::ranges::all_of(body, hex)) ||
           (text[1] == 'o' && std::ranges::all_of(body, octal));
}

// YAML 1.2 の core schema の数（符号つきの 10 進・`0x` `0o`・`.inf` `.nan`）。
[[nodiscard]] constexpr bool is_number(std::string_view text) noexcept
{
    const bool signed_text = text.starts_with('-') || text.starts_with('+');
    const std::string_view magnitude = signed_text ? text.substr(1U) : text;
    return is_prefixed(text) || lex_in<yaml_specials>(magnitude) || is_decimal(magnitude);
}

[[nodiscard]] std::optional<SyntaxRole> scalar_role(std::string_view text, bool key) noexcept
{
    if (key)
    {
        return SyntaxRole::type;
    }
    if (lex_in<yaml_constants>(text))
    {
        return SyntaxRole::constant;
    }
    return is_number(text) ? std::optional{SyntaxRole::number} : std::nullopt;
}

// ---- 値の字句 ----

// at の `:` が値の区切り（後ろが空白か行末）か。
[[nodiscard]] constexpr bool separates(std::string_view line, std::size_t at) noexcept
{
    return line[at] == ':' && (at + 1U == line.size() || lex_is_space(line[at + 1U]));
}

// 裸のスカラーを終わらせるか: 空白の後の `#`・区切りの `:`・flow の中の `, [ ] { }`。
[[nodiscard]] constexpr bool ends_scalar(std::string_view line, std::size_t at,
                                         std::size_t depth) noexcept
{
    if (line[at] == '#' && lex_is_space(line[at - 1U]))
    {
        return true;
    }
    return separates(line, at) || (depth > 0U && flow_indicators.contains(line[at]));
}

// 裸のスカラー。後ろの空白を除いた全体が定数か数なら塗る。flow の中で後ろに区切りの `:` が
// 続けばキーで type。
std::size_t lex_scalar(LexCursor &cursor, std::size_t at, std::size_t depth)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && !ends_scalar(line, end, depth))
    {
        ++end;
    }
    std::size_t last = end;
    while (last > at && lex_is_space(line[last - 1U]))
    {
        --last;
    }
    const bool key = depth > 0U && end < line.size() && line[end] == ':';
    const std::optional<SyntaxRole> role = scalar_role(line.substr(at, last - at), key);
    if (role.has_value())
    {
        cursor.mark(at, last, role.value());
    }
    return end;
}

// `'…'` の閉じの直後（`''` は引用符 1 つ）。閉じなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t single_close(std::string_view line, std::size_t open) noexcept
{
    for (std::size_t at = open + 1U; at < line.size(); ++at)
    {
        if (line[at] == '\'' && lex_next_of(line, at) != '\'')
        {
            return at + 1U;
        }
        at += line[at] == '\'' ? 1U : 0U;
    }
    return lex_unclosed;
}

// `"…"`（`\` で次の 1 バイトを飛ばす）と `'…'` は string。閉じなければ行末まで string。
std::size_t lex_quoted(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close =
        line[at] == '"' ? lex_closing_quote(line, at) : single_close(line, at);
    const std::size_t end = close == lex_unclosed ? line.size() : close;
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

// `&name` `*name` は keyword。名前は空白と flow の記号の手前まで。
std::size_t lex_anchor(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && !lex_is_space(line[end]) && !flow_indicators.contains(line[end]))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

// タグ `!` `!!str` は空白の手前まで区間なし（後続）。
[[nodiscard]] constexpr std::size_t skip_tag(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && !lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

std::size_t lex_token(LexCursor &cursor, std::size_t at, std::size_t depth)
{
    const std::string_view line = cursor.line();
    const char value = line[at];
    if (value == '"' || value == '\'')
    {
        return lex_quoted(cursor, at);
    }
    if (value == '&' || value == '*')
    {
        return lex_anchor(cursor, at);
    }
    if (separates(line, at))
    {
        return at + 1U;
    }
    return value == '!' ? skip_tag(line, at) : lex_scalar(cursor, at, depth);
}

// flow の記号の連なりは operators。`[` `{` で深さを足し、`]` `}` で引く（1 行の中だけ）。
std::size_t lex_flow(LexCursor &cursor, std::size_t at, std::size_t &depth)
{
    const std::string_view line = cursor.line();
    std::size_t end = at;
    for (; end < line.size() && flow_indicators.contains(line[end]); ++end)
    {
        const bool opens = line[end] == '[' || line[end] == '{';
        const bool closes = (line[end] == ']' || line[end] == '}') && depth > 0U;
        depth = opens ? depth + 1U : depth - (closes ? 1U : 0U);
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// ブロックスカラーの印 `|` `>`（後ろの `+ -` と数字も）は operators で、次の行からその中に入る。
// 中の深さ scope はキーの桁（`- ` を飛ばした後）か、キーが無ければ最後のリストの記号の桁（#842 の
// レビュー I-1）。印の後ろが空白か行末でなければ裸のスカラー。
std::size_t lex_block_header(LexCursor &cursor, std::size_t at, std::size_t scope)
{
    const std::string_view line = cursor.line();
    std::size_t end = at + 1U;
    while (end < line.size() && (lex_is_digit(line[end]) || line[end] == '-' || line[end] == '+'))
    {
        ++end;
    }
    if (end < line.size() && !lex_is_space(line[end]))
    {
        return lex_scalar(cursor, at, 0U);
    }
    cursor.mark(at, end, SyntaxRole::operators);
    cursor.leave(block_state(scope));
    return end;
}

std::size_t lex_comment(LexCursor &cursor, std::size_t at)
{
    const std::size_t end = cursor.line().size();
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

// 値の 1 字句。scope は行の頭（区切り・リストの記号・キー）の後の最初の字句（タグとアンカーの後ろ
// も）ならブロックスカラーの深さ、そうでなければ lex_unclosed。
std::size_t lex_value(LexCursor &cursor, std::size_t at, std::size_t &depth, std::size_t scope)
{
    const std::string_view line = cursor.line();
    const char value = line[at];
    if (value == '#' && (at == 0U || lex_is_space(line[at - 1U])))
    {
        return lex_comment(cursor, at);
    }
    if (scope != lex_unclosed && depth == 0U && (value == '|' || value == '>'))
    {
        return lex_block_header(cursor, at, scope);
    }
    return flow_indicators.contains(value) ? lex_flow(cursor, at, depth)
                                           : lex_token(cursor, at, depth);
}

// ---- 行の頭 ----

// 文書の区切り `---` `...`（行頭・後ろが空白か行末）。
[[nodiscard]] constexpr bool is_document_marker(std::string_view line) noexcept
{
    const std::string_view head = line.substr(0U, 3U);
    return (head == "---" || head == "...") && (line.size() == 3U || lex_is_space(line[3U]));
}

// from から、区切りの `:` の位置。空白の後の `#`（コメント）より前に無ければ lex_unclosed。
[[nodiscard]] constexpr std::size_t key_colon(std::string_view line, std::size_t from) noexcept
{
    for (std::size_t at = from + 1U; at < line.size(); ++at)
    {
        if (line[at] == '#' && lex_is_space(line[at - 1U]))
        {
            return lex_unclosed;
        }
        if (separates(line, at))
        {
            return at;
        }
    }
    return lex_unclosed;
}

// at から始まる裸のキーは type。キーでなければ at を、キーなら区切りの `:` の後ろを返す。
std::size_t lex_key(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (at >= line.size() || key_indicators.contains(line[at]))
    {
        return at;
    }
    const std::size_t colon = key_colon(line, at);
    if (colon == lex_unclosed)
    {
        return at;
    }
    std::size_t end = colon;
    while (end > at && lex_is_space(line[end - 1U]))
    {
        --end;
    }
    cursor.mark(at, end, SyntaxRole::type);
    return colon + 1U;
}

// 行の頭: 文書の区切り、または字下げの後のリストの記号 `- ` の連なりとキー。値の始まりを返し、
// scope にブロックスカラーの深さ（キーの桁・無ければ最後のリストの記号の桁か字下げ）を置く。
std::size_t lex_head(LexCursor &cursor, std::size_t &scope)
{
    const std::string_view line = cursor.line();
    if (is_document_marker(line))
    {
        cursor.mark(0U, 3U, SyntaxRole::keyword);
        return 3U;
    }
    std::size_t at = skip_spaces(line, 0U);
    scope = at;
    while (at < line.size() && line[at] == '-' &&
           (at + 1U == line.size() || lex_is_space(line[at + 1U])))
    {
        cursor.mark(at, at + 1U, SyntaxRole::operators);
        scope = at;
        at = skip_spaces(line, at + 1U);
    }
    const std::size_t value = lex_key(cursor, at);
    scope = value == at ? scope : at;
    return value;
}
} // namespace

void lex_yaml(LexCursor &cursor, const LexState &in)
{
    if (continues_block(cursor, in))
    {
        return;
    }
    const std::string_view line = cursor.line();
    std::size_t depth = 0U;
    std::size_t scope = 0U;
    for (std::size_t at = lex_head(cursor, scope); at < line.size();)
    {
        if (lex_is_space(line[at]))
        {
            ++at;
            continue;
        }
        // タグ `!!str` とアンカー `&a` の後ろの `|` `>` もブロックスカラーの印。
        const bool prefix = line[at] == '!' || line[at] == '&';
        at = lex_value(cursor, at, depth, scope);
        scope = prefix ? scope : lex_unclosed;
    }
}
} // namespace nenenib::core
