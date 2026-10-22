#include "LexCsharp.hpp"

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

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。予約語は C# の言語仕様の
// keywords と、Vim の cs.vim が常に塗る文脈キーワード（async await nameof partial record required
// when where yield）。get set init value と LINQ の語は Vim も文脈でしか塗らないので入れない。
constexpr std::array<std::string_view, 67> csharp_keywords{
    "abstract",  "as",         "async",     "await",  "base",     "break",     "case",
    "catch",     "checked",    "class",     "const",  "continue", "default",   "delegate",
    "do",        "else",       "enum",      "event",  "explicit", "extern",    "finally",
    "fixed",     "for",        "foreach",   "goto",   "if",       "implicit",  "in",
    "interface", "internal",   "is",        "lock",   "nameof",   "namespace", "new",
    "operator",  "out",        "override",  "params", "partial",  "private",   "protected",
    "public",    "readonly",   "record",    "ref",    "required", "return",    "sealed",
    "sizeof",    "stackalloc", "static",    "struct", "switch",   "this",      "throw",
    "try",       "typeof",     "unchecked", "unsafe", "using",    "virtual",   "volatile",
    "when",      "where",      "while",     "yield"};
// 組み込みの型（Vim の csType と同じく var と dynamic を含む）。
constexpr std::array<std::string_view, 20> csharp_types{
    "bool",  "byte",   "char",  "decimal", "double", "dynamic", "float", "int",    "long", "nint",
    "nuint", "object", "sbyte", "short",   "string", "uint",    "ulong", "ushort", "var",  "void"};
constexpr std::array<std::string_view, 3> csharp_constants{"false", "null", "true"};
// 前処理の行のうち、行の全体を keyword にするもの（Vim の csPreProcRegion と csPreProcDiagnostic
// は `//` を含めて行末まで）。ほかの指令は `//` からコメント。
constexpr std::array<std::string_view, 4> whole_directives{"endregion", "error", "region",
                                                           "warning"};
static_assert(std::ranges::is_sorted(csharp_keywords));
static_assert(std::ranges::is_sorted(csharp_types));
static_assert(std::ranges::is_sorted(csharp_constants));
static_assert(std::ranges::is_sorted(whole_directives));

constexpr std::string_view operator_characters = "+-*%=<>!&|^~?:.,;()[]{}";
constexpr std::string_view verbatim_mark = "@";
constexpr std::string_view interpolated_verbatim_mark = "$@";
constexpr std::size_t raw_quotes = 3U;
// text_step が「この引用符で閉じる」を返す印。
constexpr std::size_t closes_here = std::string_view::npos;

[[nodiscard]] constexpr bool starts_name(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

// at から引用符が続く数。
[[nodiscard]] constexpr std::size_t quote_run(std::string_view line, std::size_t at) noexcept
{
    std::size_t end = at;
    while (end < line.size() && line[end] == '"')
    {
        ++end;
    }
    return end - at;
}

// 文字列の直後の UTF-8 の接尾辞 `u8` も文字列に含める（Vim の csString の end）。
[[nodiscard]] constexpr std::size_t with_suffix(std::string_view line, std::size_t end) noexcept
{
    return line.substr(end).starts_with("u8") ? end + 2U : end;
}

// ---- 文字列 ----

// 補間の穴 `{…}` の中で at から始まる引用の字句（`"…"` `'…'`）を飛ばした次の位置。閉じなければ
// lex_unclosed。
[[nodiscard]] constexpr std::size_t skip_nested(std::string_view line, std::size_t at) noexcept
{
    const bool quoted = line[at] == '"' || line[at] == '\'';
    return quoted ? lex_closing_quote(line, at) : at + 1U;
}

// 補間の穴の中の 1 バイト。深さを動かして次の位置を返す。
[[nodiscard]] constexpr std::size_t hole_step(std::string_view line, std::size_t at,
                                              std::size_t &depth) noexcept
{
    depth += line[at] == '{' ? 1U : 0U;
    depth -= line[at] == '}' ? 1U : 0U;
    return skip_nested(line, at);
}

// 補間の穴の開き `{`（`{{` は文字）。深さを置いて次の位置を返す。
[[nodiscard]] constexpr std::size_t hole_open(std::string_view line, std::size_t at,
                                              std::size_t &depth) noexcept
{
    const bool escaped = lex_next_of(line, at) == '{';
    depth = escaped ? 0U : 1U;
    return at + (escaped ? 2U : 1U);
}

// 穴の外の 1 バイトを読んだ次の位置。閉じの引用符なら closes_here。
[[nodiscard]] constexpr std::size_t text_step(std::string_view line, std::size_t at,
                                              bool verbatim) noexcept
{
    if (line[at] == '"')
    {
        return verbatim && lex_next_of(line, at) == '"' ? at + 2U : closes_here;
    }
    return at + (line[at] == '\\' && !verbatim ? 2U : 1U);
}

// from から閉じの引用符を探し、その直後を返す。逐語的（verbatim）なら `""` は閉じず `\`
// は飛ばさない。 補間（holes）なら `{{` は文字で、`{…}`
// の中（入れ子の引用符を含む）は閉じを探さない。
[[nodiscard]] constexpr std::size_t quoted_close(std::string_view line, std::size_t from,
                                                 bool verbatim, bool holes) noexcept
{
    std::size_t depth = 0U;
    std::size_t at = from;
    while (at < line.size())
    {
        if (depth > 0U)
        {
            at = hole_step(line, at, depth);
            continue;
        }
        if (holes && line[at] == '{')
        {
            at = hole_open(line, at, depth);
            continue;
        }
        const std::size_t next = text_step(line, at, verbatim);
        if (next == closes_here)
        {
            return at + 1U;
        }
        at = next;
    }
    return lex_unclosed;
}

// begin（`$` `@` の接頭辞の先頭）から open の引用符の通常の文字列。閉じなければ行末まで error
// （通常の文字列と補間は行をまたがない）。
std::size_t mark_regular(LexCursor &cursor, std::size_t begin, std::size_t open, bool holes)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quoted_close(line, open + 1U, false, holes);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::error);
        return line.size();
    }
    const std::size_t last = with_suffix(line, end);
    cursor.mark(begin, last, SyntaxRole::string);
    return last;
}

// 逐語的文字列。閉じなければ行末まで string で、delimiter に印を持って次の行へ。
std::size_t mark_verbatim(LexCursor &cursor, std::size_t begin, std::size_t open, bool holes)
{
    const std::string_view line = cursor.line();
    const std::size_t end = quoted_close(line, open + 1U, true, holes);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(
            lex_entered(LexKind::raw_string, holes ? interpolated_verbatim_mark : verbatim_mark));
        return line.size();
    }
    const std::size_t last = with_suffix(line, end);
    cursor.mark(begin, last, SyntaxRole::string);
    return last;
}

// 生の文字列（開きの引用符 3 つ以上）。閉じは同じ数の引用符の列。区切りは raw_delimiter_limit
// 文字までで、それより長い開きは 16 個の列で閉じる（限界）。閉じなければ行末まで string
// で次の行へ。
std::size_t mark_raw(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::size_t count = std::min(quote_run(line, open), raw_delimiter_limit);
    const std::string_view quotes = line.substr(open, count);
    const std::size_t close = lex_find(line, open + quote_run(line, open), quotes);
    if (close == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, quotes));
        return line.size();
    }
    const std::size_t last = with_suffix(line, close + count);
    cursor.mark(begin, last, SyntaxRole::string);
    return last;
}

// begin の接頭辞の後ろ open の引用符から始まる文字列（引用符 3 つ以上なら生・`""` は空の文字列）。
std::size_t mark_string(LexCursor &cursor, std::size_t begin, std::size_t open, bool holes)
{
    if (quote_run(cursor.line(), open) >= raw_quotes)
    {
        return mark_raw(cursor, begin, open);
    }
    return mark_regular(cursor, begin, open, holes);
}

std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    return mark_string(cursor, at, at, false);
}

// 文字 `'a'` `'\n'`。閉じなければ行末まで error。
std::size_t lex_character(LexCursor &cursor, std::size_t at)
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

// `@"…"` `@$"…"` と逐語的な識別子 `@class`（区間なし）。ほかの `@` は演算子。
std::size_t lex_at(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    if (next == '"')
    {
        return mark_verbatim(cursor, at, at + 1U, false);
    }
    if (next == '$' && lex_next_of(line, at + 1U) == '"')
    {
        return mark_verbatim(cursor, at, at + 2U, true);
    }
    if (starts_name(next))
    {
        return lex_word_end(line, at + 1U);
    }
    cursor.mark(at, at + 1U, SyntaxRole::operators);
    return at + 1U;
}

// `$"…"` `$@"…"` `$$"""…"""`。`$` の後ろに引用符が無ければ演算子。
std::size_t lex_dollar(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    std::size_t open = at;
    while (open < line.size() && line[open] == '$')
    {
        ++open;
    }
    if (open < line.size() && line[open] == '"')
    {
        return mark_string(cursor, at, open, true);
    }
    if (line.substr(open).starts_with("@\""))
    {
        return mark_verbatim(cursor, at, open + 1U, true);
    }
    cursor.mark(at, open, SyntaxRole::operators);
    return open;
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

// `/* … */` を from から塗る（at はコメントの始まり・閉じの探しは from から）。閉じなければ行末まで
// comment で次の行へ。
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

// 数字の途中の 1 バイトが数字を続けるか。`_` と接尾辞（`m` `UL` `f`）は識別子の文字に含まれ、
// `.` は直後が桁のときだけ（`1..2` の範囲を切る）、符号は 16 進でない数の指数の直後だけ。
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
    const std::string_view line = cursor.line();
    if (lex_next_of(line, at) == '/')
    {
        cursor.mark(at, line.size(), SyntaxRole::comment); // `//` と `///` のドキュメントコメント
        return line.size();
    }
    if (lex_next_of(line, at) == '*')
    {
        return mark_block_comment(cursor, at, at + 2U);
    }
    return lex_operators(cursor, at);
}

// ---- 識別子 ----

// 予約語・型・定数の表の順に引き、どれでもなく直後が `(` なら関数（C / C++ と同じ）。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool called) noexcept
{
    if (std::ranges::binary_search(csharp_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (std::ranges::binary_search(csharp_types, word))
    {
        return SyntaxRole::type;
    }
    if (std::ranges::binary_search(csharp_constants, word))
    {
        return SyntaxRole::constant;
    }
    return called ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const bool called = end < line.size() && line[end] == '(';
    const std::optional<SyntaxRole> role = word_role(line.substr(at, end - at), called);
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
    constexpr std::array<std::pair<char, LexHandler>, 5> marks{{{'"', lex_string},
                                                                {'\'', lex_character},
                                                                {'/', lex_slash},
                                                                {'@', lex_at},
                                                                {'$', lex_dollar}}};
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

// バイト → 字句の関数の表（ADR 0156 の決定 6）。空白・`#`（行の途中）・`\`・0x80 以上は
// skip_character。
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of(static_cast<char>(index));
    }
    return table;
}();

// ---- 前処理の行 ----

// 前後の空白を除いた [begin, end) を keyword で塗る。
void mark_directive(LexCursor &cursor, std::size_t begin, std::size_t end)
{
    const std::string_view line = cursor.line();
    while (end > begin && lex_is_space(line[end - 1U]))
    {
        --end;
    }
    cursor.mark(begin, end, SyntaxRole::keyword);
}

// `#` から行末までを keyword とし、`#region` `#error` などでなければ `//` からをコメントにする。
void lex_directive(LexCursor &cursor, std::size_t hash)
{
    const std::string_view line = cursor.line();
    const std::size_t name = lex_first_visible(line.substr(hash + 1U)) + hash + 1U;
    const std::string_view word = line.substr(name, lex_word_end(line, name) - name);
    const std::size_t comment = lex_find(line, hash, "//");
    if (std::ranges::binary_search(whole_directives, word) || comment == lex_unclosed)
    {
        mark_directive(cursor, hash, line.size());
        return;
    }
    mark_directive(cursor, hash, comment);
    cursor.mark(comment, line.size(), SyntaxRole::comment);
}

// ---- 行をまたぐ状態の続き ----

// 文字列の中で始まる行。閉じまでが string で、閉じなければ行全体が string のまま。
std::size_t resume_string(LexCursor &cursor, const LexState &in, std::size_t end)
{
    const std::string_view line = cursor.line();
    if (end == lex_unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    const std::size_t last = with_suffix(line, end);
    cursor.mark(0U, last, SyntaxRole::string);
    return last;
}

// C# の raw_string の区切り（`@` `$@` と引用符 3 つ以上の列）でなければ C# の状態でないので、
// 行頭から読む。
std::size_t resume_raw(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::string_view delimiter = lex_delimiter(in);
    if (delimiter == verbatim_mark || delimiter == interpolated_verbatim_mark)
    {
        return resume_string(cursor, in,
                             quoted_close(line, 0U, true, delimiter == interpolated_verbatim_mark));
    }
    if (delimiter.size() < raw_quotes || quote_run(delimiter, 0U) != delimiter.size())
    {
        return 0U;
    }
    const std::size_t close = lex_find(line, 0U, delimiter);
    return resume_string(cursor, in, close == lex_unclosed ? close : close + delimiter.size());
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

[[nodiscard]] bool starts_directive(const LexState &in, std::string_view line) noexcept
{
    const std::size_t hash = lex_first_visible(line);
    return in.kind == LexKind::plain && hash < line.size() && line[hash] == '#';
}
} // namespace

void lex_csharp(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    if (starts_directive(in, line))
    {
        lex_directive(cursor, lex_first_visible(line));
        return;
    }
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
