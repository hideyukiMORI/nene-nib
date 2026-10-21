#include "LexHtml.hpp"

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
constexpr std::string_view comment_open = "<!--";
constexpr std::string_view comment_close = "-->";
constexpr std::string_view cdata_open = "<![CDATA[";
constexpr std::string_view cdata_close = "]]>";
// 中身を分類しない要素の閉じタグの頭（html だけ・大小を問わない）。
constexpr std::array<std::string_view, 2> raw_closes{"</script", "</style"};
// 実体参照の名前の最大の長さ（Vim の htmlSpecialChar の `\{1,8}`）。
constexpr std::size_t entity_limit = 8U;
// 属性名を終わらせる文字（空白のほかに）。
constexpr std::string_view attribute_stops = "=>/?\"'<";

[[nodiscard]] constexpr char lowered(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value | '\x20') : value;
}

[[nodiscard]] constexpr bool is_alnum(char value) noexcept
{
    return lex_is_word(value) && value != '_';
}

[[nodiscard]] constexpr bool starts_name(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

// タグ名の文字（名前空間の `a:b` と `-` `.` を含む）。
[[nodiscard]] constexpr bool is_name(char value) noexcept
{
    return lex_is_word(value) || value == '-' || value == ':' || value == '.';
}

// text を ASCII の小文字に畳むと name（小文字）に等しいか。
[[nodiscard]] constexpr bool same_name(std::string_view text, std::string_view name) noexcept
{
    return std::ranges::equal(text, name,
                              [](char left, char right) { return lowered(left) == right; });
}

// ---- 行をまたぐ区間（コメントと CDATA） ----

[[nodiscard]] constexpr SyntaxRole region_role(LexKind kind) noexcept
{
    return kind == LexKind::block_comment ? SyntaxRole::comment : SyntaxRole::string;
}

[[nodiscard]] constexpr std::string_view region_close(LexKind kind) noexcept
{
    return kind == LexKind::block_comment ? comment_close : cdata_close;
}

// begin から、from の後ろの閉じ（コメントは `-->`・CDATA は `]]>`）までを塗る。閉じなければ
// 行末までで、次の行へ kind を渡す。
std::size_t mark_region(LexCursor &cursor, std::size_t begin, std::size_t from, LexKind kind)
{
    const std::string_view line = cursor.line();
    const std::string_view close = region_close(kind);
    const std::size_t found = lex_find(line, from, close);
    if (found == lex_unclosed)
    {
        cursor.mark(begin, line.size(), region_role(kind));
        const std::string_view delimiter = kind == LexKind::raw_string ? close : std::string_view{};
        cursor.leave(lex_entered(kind, delimiter));
        return line.size();
    }
    cursor.mark(begin, found + close.size(), region_role(kind));
    return found + close.size();
}

// ---- script と style の中身 ----

// from から close（`</script` か `</style`・大小を問わず、後ろが名前の文字でない）の位置。
[[nodiscard]] constexpr std::size_t closing_tag(std::string_view line, std::size_t from,
                                                std::string_view close) noexcept
{
    for (std::size_t at = line.find('<', from); at != std::string_view::npos;
         at = line.find('<', at + 1U))
    {
        const std::size_t after = at + close.size();
        const bool named = after <= line.size() && same_name(line.substr(at, close.size()), close);
        if (named && (after == line.size() || !is_name(line[after])))
        {
            return at;
        }
    }
    return lex_unclosed;
}

// at から閉じタグの手前までは分類しない。閉じなければ次の行へ raw_string（delimiter は close）。
std::size_t skip_raw(LexCursor &cursor, std::size_t at, std::string_view close)
{
    const std::size_t found = closing_tag(cursor.line(), at, close);
    if (found == lex_unclosed)
    {
        cursor.leave(lex_entered(LexKind::raw_string, close));
        return cursor.line().size();
    }
    return found;
}

// 開きタグの頭（`<script` など）が中身を分類しない要素なら、その閉じタグの頭。でなければ空。
[[nodiscard]] constexpr std::string_view raw_close(std::string_view head) noexcept
{
    for (const std::string_view close : raw_closes)
    {
        const std::string_view name = close.substr(2U);
        if (head.size() == name.size() + 1U && same_name(head.substr(1U), name))
        {
            return close;
        }
    }
    return {};
}

// ---- タグの中 ----

// at が開いたタグを閉じる印（`>` `/>` `?>`）ならその長さ、でなければ 0。
[[nodiscard]] constexpr std::size_t closer_length(std::string_view line, std::size_t at) noexcept
{
    if (line[at] == '>')
    {
        return 1U;
    }
    const bool mark = line[at] == '/' || line[at] == '?';
    return mark && lex_next_of(line, at) == '>' ? 2U : 0U;
}

// 属性の値の引用符。HTML の引用符にエスケープは無く、閉じなければ行末まで string（決定 4）。
std::size_t lex_quoted(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close = line.find(line[at], at + 1U);
    const std::size_t end = close == std::string_view::npos ? line.size() : close + 1U;
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

[[nodiscard]] constexpr bool ends_unquoted(char value) noexcept
{
    return lex_is_space(value) || value == '>' || value == '<' || value == '"' || value == '\'';
}

// `=` の後ろの空白を飛ばし、引用符の無い値（空白か `>` の手前まで）を string で塗る。値が引用符
// で始まれば、引用符の位置を返す。
std::size_t lex_equals(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t begin = at + 1U;
    while (begin < line.size() && lex_is_space(line[begin]))
    {
        ++begin;
    }
    std::size_t end = begin;
    while (end < line.size() && !ends_unquoted(line[end]))
    {
        ++end;
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

// 属性名は type。名前にならない 1 字（単独の `/` `?`）は飛ばす。
std::size_t lex_attribute(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t end = at;
    while (end < line.size() && !lex_is_space(line[end]) && !attribute_stops.contains(line[end]))
    {
        ++end;
    }
    if (end == at)
    {
        return at + 1U;
    }
    cursor.mark(at, end, SyntaxRole::type);
    return end;
}

std::size_t lex_inside_tag(LexCursor &cursor, std::size_t at)
{
    const char value = cursor.line()[at];
    if (value == '"' || value == '\'')
    {
        return lex_quoted(cursor, at);
    }
    return value == '=' ? lex_equals(cursor, at) : lex_attribute(cursor, at);
}

// タグの頭の後ろの属性を、閉じの印の直後まで読む。行末か次の `<` で止まれば、そこを返す。
std::size_t lex_attributes(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    while (at < line.size() && line[at] != '<')
    {
        const std::size_t closer = closer_length(line, at);
        if (closer != 0U)
        {
            cursor.mark(at, at + closer, SyntaxRole::keyword);
            return at + closer;
        }
        at = lex_is_space(line[at]) ? at + 1U : lex_inside_tag(cursor, at);
    }
    return at;
}

// at の `<` の後ろ（`</` と xml の `<?` は 2 字の後ろ）に名前が続けばタグ。頭は keyword。html の
// `<script>` `<style>`（`/>` で閉じない開きタグ）の後ろは閉じタグまで分類しない。
std::size_t lex_tag(LexCursor &cursor, std::size_t at, bool xml)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    std::size_t name_end = next == '/' || (xml && next == '?') ? at + 2U : at + 1U;
    if (name_end >= line.size() || !starts_name(line[name_end]))
    {
        return at + 1U;
    }
    while (name_end < line.size() && is_name(line[name_end]))
    {
        ++name_end;
    }
    cursor.mark(at, name_end, SyntaxRole::keyword);
    const std::size_t end = lex_attributes(cursor, name_end);
    const std::string_view close =
        xml ? std::string_view{} : raw_close(line.substr(at, name_end - at));
    const bool opened = !close.empty() && line[end - 1U] == '>' && line[end - 2U] != '/';
    return opened ? skip_raw(cursor, end, close) : end;
}

// ---- テキスト ----

// `<!DOCTYPE …>` などの宣言は `>` まで（無ければ行末まで）keyword。
std::size_t lex_declaration(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close = line.find('>', at);
    const std::size_t end = close == std::string_view::npos ? line.size() : close + 1U;
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

std::size_t lex_angle(LexCursor &cursor, std::size_t at, bool xml)
{
    const std::string_view rest = cursor.line().substr(at);
    if (rest.starts_with(comment_open))
    {
        return mark_region(cursor, at, at + comment_open.size(), LexKind::block_comment);
    }
    if (xml && rest.starts_with(cdata_open))
    {
        return mark_region(cursor, at, at + cdata_open.size(), LexKind::raw_string);
    }
    return rest.starts_with("<!") ? lex_declaration(cursor, at) : lex_tag(cursor, at, xml);
}

// `&name;` `&#123;` `&#x1F;` は constant（`&` か `&#` の後ろは 1〜8 文字の英数字）。ほかの `&` は
// 分類しない。
std::size_t lex_entity(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t name = lex_next_of(line, at) == '#' ? at + 2U : at + 1U;
    std::size_t end = name;
    while (end < line.size() && end - name < entity_limit && is_alnum(line[end]))
    {
        ++end;
    }
    if (end == name || end >= line.size() || line[end] != ';')
    {
        return at + 1U;
    }
    cursor.mark(at, end + 1U, SyntaxRole::constant);
    return end + 1U;
}

std::size_t lex_text(LexCursor &cursor, std::size_t at, bool xml)
{
    const char value = cursor.line()[at];
    if (value == '<')
    {
        return lex_angle(cursor, at, xml);
    }
    return value == '&' ? lex_entity(cursor, at) : at + 1U;
}

// ---- 行をまたぐ状態の続き ----

// raw_string の中で始まる行。xml の CDATA は閉じまで string、html の script / style は閉じタグ
// まで分類しない。ほかの区切りはこの言語の状態でないので行頭から読む。
std::size_t resume_raw(LexCursor &cursor, const LexState &in, bool xml)
{
    const std::string_view delimiter = lex_delimiter(in);
    if (xml)
    {
        return delimiter == cdata_close ? mark_region(cursor, 0U, 0U, LexKind::raw_string) : 0U;
    }
    const bool raw = std::ranges::find(raw_closes, delimiter) != raw_closes.end();
    return raw ? skip_raw(cursor, 0U, delimiter) : 0U;
}

std::size_t resumed(LexCursor &cursor, const LexState &in, bool xml)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return mark_region(cursor, 0U, 0U, LexKind::block_comment);
    case LexKind::raw_string:
        return resume_raw(cursor, in, xml);
    }
    std::unreachable();
}
} // namespace

void lex_html(LexCursor &cursor, const LexState &in, Language language)
{
    const bool xml = language == Language::xml;
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in, xml); at < line.size();)
    {
        at = lex_text(cursor, at, xml);
    }
}
} // namespace nenenib::core
