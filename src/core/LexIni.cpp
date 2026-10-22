#include "LexIni.hpp"

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
// 字句の関数（gdresource の値）。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。
constexpr std::array<std::string_view, 3> resource_constants{"false", "null", "true"};
static_assert(std::ranges::is_sorted(resource_constants));

// gdresource の見出しの外を示す深さ（見出しの中では `[` `]` の入れ子の深さ）。
constexpr std::size_t outside_header = lex_unclosed;
// gdresource の行をまたぐ文字列の区切り。
constexpr std::string_view resource_quote = "\"";

[[nodiscard]] constexpr bool starts_name(char value) noexcept
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

[[nodiscard]] constexpr std::size_t skip_back_spaces(std::string_view line,
                                                     std::size_t end) noexcept
{
    while (end > 0U && lex_is_space(line[end - 1U]))
    {
        --end;
    }
    return end;
}

std::size_t mark_comment(LexCursor &cursor, std::size_t at)
{
    const std::size_t end = cursor.line().size();
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

// ---- dosini ----

[[nodiscard]] constexpr std::size_t digits_end(std::string_view value, std::size_t at) noexcept
{
    while (at < value.size() && lex_is_digit(value[at]))
    {
        ++at;
    }
    return at;
}

// 値の全体が Vim の dosiniNumber の `\d\+` か `\d*\.\d\+` の形（`12` `.5` `1.5`）か。
[[nodiscard]] constexpr bool is_ini_number(std::string_view value) noexcept
{
    const std::size_t head = digits_end(value, 0U);
    if (head == value.size())
    {
        return head > 0U;
    }
    const bool fraction = value[head] == '.' && head + 1U < value.size();
    return fraction && digits_end(value, head + 1U) == value.size();
}

// 値の全体が Vim の dosiniNumber の `\d\+e[+-]\=\d\+` の形（`1e5` `2E-3`）か。
[[nodiscard]] constexpr bool is_ini_exponent(std::string_view value) noexcept
{
    std::size_t at = digits_end(value, 0U);
    if (at == 0U || at == value.size() || (value[at] | '\x20') != 'e')
    {
        return false;
    }
    ++at;
    at += at < value.size() && (value[at] == '+' || value[at] == '-') ? 1U : 0U;
    return at < value.size() && digits_end(value, at) == value.size();
}

// from から値の中の閉じた `"…"` を string で塗る（エスケープなし・閉じなければ塗らない）。
void mark_ini_quotes(LexCursor &cursor, std::size_t from)
{
    const std::string_view line = cursor.line();
    for (std::size_t open = line.find('"', from); open != std::string_view::npos;)
    {
        const std::size_t close = line.find('"', open + 1U);
        if (close == std::string_view::npos)
        {
            return;
        }
        cursor.mark(open, close + 1U, SyntaxRole::string);
        open = line.find('"', close + 1U);
    }
}

// `=` の後ろの値。前後の空白を除いた全体が数なら number、そうでなければ `"…"` だけを string。
void mark_ini_value(LexCursor &cursor, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t begin = skip_spaces(line, from);
    const std::size_t end = std::max(begin, skip_back_spaces(line, line.size()));
    const std::string_view value = line.substr(begin, end - begin);
    if (is_ini_number(value) || is_ini_exponent(value))
    {
        cursor.mark(begin, end, SyntaxRole::number);
        return;
    }
    mark_ini_quotes(cursor, begin);
}

void lex_dosini(LexCursor &cursor)
{
    const std::string_view line = cursor.line();
    const std::size_t first = lex_first_visible(line);
    if (first == line.size())
    {
        return;
    }
    if (line[first] == ';' || line[first] == '#')
    {
        static_cast<void>(mark_comment(cursor, first));
        return;
    }
    if (line[first] == '[')
    {
        const std::size_t close = line.find(']', first);
        cursor.mark(first, close == std::string_view::npos ? line.size() : close + 1U,
                    SyntaxRole::keyword);
        return;
    }
    const std::size_t equals = line.find('=', first);
    if (equals == std::string_view::npos)
    {
        return;
    }
    cursor.mark(first, skip_back_spaces(line, equals), SyntaxRole::type);
    cursor.mark(equals, equals + 1U, SyntaxRole::operators);
    mark_ini_value(cursor, equals + 1U);
}

// ---- gdresource の文字列と値 ----

// from から `"` の閉じの直後。`\` は次の 1 バイトを飛ばす。閉じなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t quote_close(std::string_view line, std::size_t from) noexcept
{
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line[at] == '"')
        {
            return at + 1U;
        }
        at += line[at] == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

// begin（`&` `^` の接頭辞の先頭）から open の `"` の文字列。閉じなければ行末まで string で、
// `"` を区切りの raw_string として次の行へ。
std::size_t mark_resource_string(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quote_close(line, open + 1U);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, resource_quote));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    return mark_resource_string(cursor, at, at);
}

// `&"…"` `^"…"`（StringName と NodePath）。後ろが `"` でなければ 1 字を飛ばす。
std::size_t lex_prefixed(LexCursor &cursor, std::size_t at)
{
    const bool quoted = lex_next_of(cursor.line(), at) == '"';
    return quoted ? mark_resource_string(cursor, at, at + 1U) : at + 1U;
}

// 数字の途中の 1 バイトが数字を続けるか（Vim の gdResourceNumber の形・符号は指数の直後だけ）。
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

// `.5` の `.` は数の始まりで、ほかの `.` は飛ばす。
std::size_t lex_dot(LexCursor &cursor, std::size_t at)
{
    return lex_is_digit(lex_next_of(cursor.line(), at)) ? lex_number(cursor, at) : at + 1U;
}

// 名前: `(` が直後に続けば呼び出しの名前で type、`true false null` は constant、ほかは区間なし。
std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    if (end < line.size() && line[end] == '(')
    {
        cursor.mark(at, end, SyntaxRole::type);
    }
    else if (lex_in<resource_constants>(line.substr(at, end - at)))
    {
        cursor.mark(at, end, SyntaxRole::constant);
    }
    return end;
}

std::size_t lex_equals(LexCursor &cursor, std::size_t at)
{
    cursor.mark(at, at + 1U, SyntaxRole::operators);
    return at + 1U;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 6> marks{{{'"', lex_string},
                                                                {'&', lex_prefixed},
                                                                {'^', lex_prefixed},
                                                                {';', mark_comment},
                                                                {'.', lex_dot},
                                                                {'=', lex_equals}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return skip_character;
}

[[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    return lex_is_word(value) ? lex_word : marked_handler(value);
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白・括弧・`,` `:`・0x80 以上は skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// ---- gdresource のキーと見出し ----

// キーの文字（識別子の文字と `/` `:` `.` `-`・`config/name` `0:0/0/physics_layer_0`）。
[[nodiscard]] constexpr bool is_key(char value) noexcept
{
    return lex_is_word(value) || value == '/' || value == ':' || value == '.' || value == '-';
}

// at からキーの文字が続き、空白の後ろに `=`（`==` でない）が続けばその `=` の位置。
[[nodiscard]] constexpr std::size_t key_equals(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = at;
    while (end < line.size() && is_key(line[end]))
    {
        ++end;
    }
    const std::size_t equals = skip_spaces(line, end);
    const bool assigned =
        end > at && equals < line.size() && line[equals] == '=' && lex_next_of(line, equals) != '=';
    return assigned ? equals : lex_unclosed;
}

// at がキーなら type と `=` を塗って `=` の後ろを返す。キーでなければ at。
std::size_t mark_key(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t equals = key_equals(line, at);
    if (equals == lex_unclosed)
    {
        return at;
    }
    cursor.mark(at, skip_back_spaces(line, equals), SyntaxRole::type);
    cursor.mark(equals, equals + 1U, SyntaxRole::operators);
    return equals + 1U;
}

// 行頭の `[name`（名前は数字で始まらない）から見出しを読み、名前の後ろを返す。`[name]` だけの
// 見出しは閉じまでを 1 区間にし、属性の続く見出しは depth を 0 にして閉じを lex_values に任せる。
std::size_t mark_header(LexCursor &cursor, std::size_t first, std::size_t &depth)
{
    const std::string_view line = cursor.line();
    const std::size_t name_end = lex_word_end(line, first + 1U);
    if (name_end < line.size() && line[name_end] == ']')
    {
        cursor.mark(first, name_end + 1U, SyntaxRole::keyword);
        return name_end + 1U;
    }
    cursor.mark(first, name_end, SyntaxRole::keyword);
    depth = 0U;
    return name_end;
}

// 行頭（空白の後）の見出しかキー。どちらでもなければ行頭の位置。
std::size_t lex_head(LexCursor &cursor, std::size_t &depth)
{
    const std::string_view line = cursor.line();
    const std::size_t first = lex_first_visible(line);
    const bool header =
        first < line.size() && line[first] == '[' && starts_name(lex_next_of(line, first));
    return header ? mark_header(cursor, first, depth) : mark_key(cursor, first);
}

// 見出しの中の 1 字。閉じの `]` は keyword で塗って見出しを出る。`[` `]` は入れ子の深さを数え、
// 空白の後ろの `name=` は属性名。値の字句として読む字なら lex_unclosed。
std::size_t header_character(LexCursor &cursor, std::size_t at, std::size_t &depth)
{
    const std::string_view line = cursor.line();
    const char value = line[at];
    if (value == ']' && depth == 0U)
    {
        cursor.mark(at, at + 1U, SyntaxRole::keyword);
        depth = outside_header;
        return at + 1U;
    }
    depth += value == '[' ? 1U : 0U;
    depth -= value == ']' ? 1U : 0U;
    const bool attribute = lex_is_space(line[at - 1U]) && is_key(value);
    const std::size_t after = attribute ? mark_key(cursor, at) : at;
    return after == at ? lex_unclosed : after;
}

// at から行末まで値の字句を読む。depth が outside_header でなければ見出しの中。
void lex_values(LexCursor &cursor, std::size_t at, std::size_t depth)
{
    const std::string_view line = cursor.line();
    while (at < line.size())
    {
        const std::size_t header =
            depth == outside_header ? lex_unclosed : header_character(cursor, at, depth);
        at = header != lex_unclosed ? header
                                    : handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}

// `"` の文字列の中で始まる行。閉じまでが string で、閉じなければ行全体が string のまま。
std::size_t resume_string(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quote_close(line, 0U);
    if (end == lex_unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, end, SyntaxRole::string);
    return end;
}

// gdresource にブロックコメントは無く、`"` でない区切りは gdresource の状態でないので行頭から読む。
[[nodiscard]] bool continues_string(const LexState &in) noexcept
{
    switch (in.kind)
    {
    case LexKind::plain:
    case LexKind::block_comment:
        return false;
    case LexKind::raw_string:
        return lex_delimiter(in) == resource_quote;
    }
    std::unreachable();
}

void lex_gdresource(LexCursor &cursor, const LexState &in)
{
    std::size_t depth = outside_header;
    const bool continued = continues_string(in);
    const std::size_t at = continued ? resume_string(cursor, in) : lex_head(cursor, depth);
    lex_values(cursor, at, depth);
}
} // namespace

void lex_ini(LexCursor &cursor, const LexState &in, Language language)
{
    if (language == Language::gdresource)
    {
        lex_gdresource(cursor, in);
        return;
    }
    lex_dosini(cursor);
}
} // namespace nenenib::core
