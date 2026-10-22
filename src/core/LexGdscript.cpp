#include "LexGdscript.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
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
// 字句の関数。読み始めの位置と、行が関数の宣言（`func` か `static func` で始まる）かを受けて、
// 読み終えた次の位置を返す（必ず進む）。宣言かは行ごとに 1 回だけ決めて渡す（1 行を O(n) に保つ）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t, bool);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。定数が予約語より先に勝つ。
constexpr std::array<std::string_view, 31> gdscript_keywords{
    "and",   "as",       "assert", "await", "break", "breakpoint", "class", "class_name",
    "const", "continue", "elif",   "else",  "enum",  "extends",    "for",   "func",
    "if",    "in",       "is",     "match", "not",   "or",         "pass",  "return",
    "self",  "signal",   "static", "super", "var",   "when",       "while"};
constexpr std::array<std::string_view, 7> gdscript_constants{"INF",   "NAN",  "PI",  "TAU",
                                                             "false", "null", "true"};
// 組み込みの型（Godot 4 の Variant の型と void・Variant・Node）。
constexpr std::array<std::string_view, 41> gdscript_types{"AABB",
                                                          "Array",
                                                          "Basis",
                                                          "Callable",
                                                          "Color",
                                                          "Dictionary",
                                                          "Node",
                                                          "NodePath",
                                                          "Object",
                                                          "PackedByteArray",
                                                          "PackedColorArray",
                                                          "PackedFloat32Array",
                                                          "PackedFloat64Array",
                                                          "PackedInt32Array",
                                                          "PackedInt64Array",
                                                          "PackedStringArray",
                                                          "PackedVector2Array",
                                                          "PackedVector3Array",
                                                          "PackedVector4Array",
                                                          "Plane",
                                                          "Projection",
                                                          "Quaternion",
                                                          "RID",
                                                          "Rect2",
                                                          "Rect2i",
                                                          "Signal",
                                                          "String",
                                                          "StringName",
                                                          "Transform2D",
                                                          "Transform3D",
                                                          "Variant",
                                                          "Vector2",
                                                          "Vector2i",
                                                          "Vector3",
                                                          "Vector3i",
                                                          "Vector4",
                                                          "Vector4i",
                                                          "bool",
                                                          "float",
                                                          "int",
                                                          "void"};
static_assert(std::ranges::is_sorted(gdscript_keywords));
static_assert(std::ranges::is_sorted(gdscript_constants));
static_assert(std::ranges::is_sorted(gdscript_types));

constexpr std::string_view operator_characters = "+-*/%=<>!&|^~.,:;()[]{}";
constexpr std::size_t triple = 3U;

[[nodiscard]] constexpr bool is_quote(char value) noexcept
{
    return value == '"' || value == '\'';
}

[[nodiscard]] constexpr bool starts_name(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

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

// end の手前の識別子の先頭。
[[nodiscard]] constexpr std::size_t word_begin(std::string_view line, std::size_t end) noexcept
{
    while (end > 0U && lex_is_word(line[end - 1U]))
    {
        --end;
    }
    return end;
}

// at の手前（空白を飛ばす）の識別子。無ければ空。
[[nodiscard]] constexpr std::string_view previous_word(std::string_view line,
                                                       std::size_t at) noexcept
{
    const std::size_t end = skip_back_spaces(line, at);
    const std::size_t begin = word_begin(line, end);
    return line.substr(begin, end - begin);
}

// ---- 文字列 ----

[[nodiscard]] constexpr bool opens_triple(std::string_view line, std::size_t open) noexcept
{
    return open + 2U < line.size() && line[open + 1U] == line[open] &&
           line[open + 2U] == line[open];
}

// from から 3 連引用符 delimiter の閉じを探し、その直後を返す。`\` は次の 1 バイトを飛ばす。
[[nodiscard]] constexpr std::size_t triple_close(std::string_view line, std::size_t from,
                                                 std::string_view delimiter) noexcept
{
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line.substr(at).starts_with(delimiter))
        {
            return at + delimiter.size();
        }
        at += line[at] == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

// begin（`&` `^` の接頭辞の先頭）から open の 3 連引用符。閉じていなければ行末まで string で
// 次の行へ。
std::size_t mark_triple(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::string_view delimiter = line.substr(open, triple);
    const std::size_t end = triple_close(line, open + triple, delimiter);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, delimiter));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

// begin から open の引用符で始まる文字列。1 つの引用符の文字列は行をまたがず、閉じていなければ
// 行末まで error。
std::size_t mark_string(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    if (opens_triple(line, open))
    {
        return mark_triple(cursor, begin, open);
    }
    const std::size_t end = lex_closing_quote(line, open);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::error);
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

std::size_t lex_string(LexCursor &cursor, std::size_t at, bool /*signature*/)
{
    return mark_string(cursor, at, at);
}

// ---- ノードのパス ----

// at からノードのパスの文字（識別子の文字と `/`）が続く終わり。
[[nodiscard]] constexpr std::size_t path_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && (lex_is_word(line[at]) || line[at] == '/'))
    {
        ++at;
    }
    return at;
}

// at の印（`$` `%`）から、from の引用符の 1 区間か名前と `/` の並びまでが constant。引用符が
// 閉じていなければ行末まで error。
std::size_t mark_node(LexCursor &cursor, std::size_t at, std::size_t from)
{
    const std::string_view line = cursor.line();
    if (from < line.size() && is_quote(line[from]))
    {
        const std::size_t end = lex_closing_quote(line, from);
        const bool closed = end != lex_unclosed;
        cursor.mark(at, closed ? end : line.size(),
                    closed ? SyntaxRole::constant : SyntaxRole::error);
        return closed ? end : line.size();
    }
    const std::size_t end = path_end(line, from);
    cursor.mark(at, end, SyntaxRole::constant);
    return std::max(end, at + 1U);
}

// `$Path/To` `$"../x"` `$%Unique`。
std::size_t lex_dollar(LexCursor &cursor, std::size_t at, bool /*signature*/)
{
    const bool unique = lex_next_of(cursor.line(), at) == '%';
    return mark_node(cursor, at, at + (unique ? 2U : 1U));
}

// at の前（空白を飛ばす）が式の始まりか: 行頭・演算子や括弧の後ろ・`self` `super` でない予約語の
// 後ろ。識別子・数字・文字列・閉じ括弧の後ろは値で、そこの `%` は剰余。
[[nodiscard]] bool starts_expression(std::string_view line, std::size_t at) noexcept
{
    const std::size_t end = skip_back_spaces(line, at);
    if (end == 0U)
    {
        return true;
    }
    const char previous = line[end - 1U];
    if (previous == ')' || previous == ']' || previous == '}' || is_quote(previous))
    {
        return false;
    }
    if (!lex_is_word(previous))
    {
        return true;
    }
    const std::string_view word = previous_word(line, end);
    return word != "self" && word != "super" && std::ranges::binary_search(gdscript_keywords, word);
}

// at の `%` が一意な名前のノード `%Name` `%"x"` の始まりか。
[[nodiscard]] bool names_unique(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    return (starts_name(next) || is_quote(next)) && starts_expression(line, at);
}

// ---- コメントと演算子と数字 ----

std::size_t lex_comment(LexCursor &cursor, std::size_t at, bool /*signature*/)
{
    const std::size_t end = cursor.line().size();
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '.' && lex_is_digit(lex_next_of(line, at));
}

// 数字の途中の 1 バイトが数字を続けるか。`_` は識別子の文字に含まれ、符号は 16 進でない数の
// 指数の直後だけ。
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

std::size_t lex_number(LexCursor &cursor, std::size_t at, bool /*signature*/)
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

// at の演算子の文字が別の字句（`.5`・`&"…"` `^"…"`・`%Name`）の始まりか。
[[nodiscard]] bool starts_other(std::string_view line, std::size_t at) noexcept
{
    const char value = line[at];
    if (value == '&' || value == '^')
    {
        return is_quote(lex_next_of(line, at));
    }
    return starts_number(line, at) || (value == '%' && names_unique(line, at));
}

// 演算子の文字の位置から始まる別の字句を読む。
std::size_t lex_other(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (line[at] == '%')
    {
        return mark_node(cursor, at, at + 1U);
    }
    return line[at] == '.' ? lex_number(cursor, at, false) : mark_string(cursor, at, at + 1U);
}

std::size_t lex_operators(LexCursor &cursor, std::size_t at, bool /*signature*/)
{
    const std::string_view line = cursor.line();
    if (starts_other(line, at))
    {
        return lex_other(cursor, at);
    }
    std::size_t end = at + 1U;
    while (end < line.size() && operator_characters.contains(line[end]) && !starts_other(line, end))
    {
        ++end;
    }
    cursor.mark(at, end, SyntaxRole::operators);
    return end;
}

// `@` の直後に名前が続けば注釈で keyword（行のどこでも・Python のデコレータと違う所）。
std::size_t lex_annotation(LexCursor &cursor, std::size_t at, bool /*signature*/)
{
    const std::string_view line = cursor.line();
    if (!starts_name(lex_next_of(line, at)))
    {
        cursor.mark(at, at + 1U, SyntaxRole::operators);
        return at + 1U;
    }
    const std::size_t end = lex_word_end(line, at + 1U);
    cursor.mark(at, end, SyntaxRole::keyword);
    return end;
}

// ---- 識別子 ----

// at の識別子が型の注釈の位置か: `->` の後ろ、`var` `const` の名前の `:` の後ろ、関数の宣言の行の
// `(` `,` に続く引数の `:` の後ろ（Vim の gdscriptTypeDecl）。
[[nodiscard]] bool typed_position(std::string_view line, std::size_t at, bool signature) noexcept
{
    const std::size_t mark = skip_back_spaces(line, at);
    if (mark >= 2U && line[mark - 2U] == '-' && line[mark - 1U] == '>')
    {
        return true;
    }
    if (mark == 0U || line[mark - 1U] != ':')
    {
        return false;
    }
    const std::size_t name_end = skip_back_spaces(line, mark - 1U);
    const std::size_t name = word_begin(line, name_end);
    const std::string_view before = previous_word(line, name);
    const std::size_t opener = skip_back_spaces(line, name);
    const bool listed = opener > 0U && (line[opener - 1U] == '(' || line[opener - 1U] == ',');
    return name < name_end &&
           (before == "var" || before == "const" || (signature && before.empty() && listed));
}

// at の識別子の前が空白をはさんだ `func` か（その名前は function）。
[[nodiscard]] bool follows_func(std::string_view line, std::size_t at) noexcept
{
    return skip_back_spaces(line, at) < at && previous_word(line, at) == "func";
}

// 定数・予約語・型の表の順に引き、どれでもなく型の注釈の位置なら型、`func` の名前なら関数。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool typed,
                                                  bool named) noexcept
{
    if (std::ranges::binary_search(gdscript_constants, word))
    {
        return SyntaxRole::constant;
    }
    if (std::ranges::binary_search(gdscript_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (typed || std::ranges::binary_search(gdscript_types, word))
    {
        return SyntaxRole::type;
    }
    return named ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at, bool signature)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::optional<SyntaxRole> role = word_role(
        line.substr(at, end - at), typed_position(line, at, signature), follows_func(line, at));
    if (role.has_value())
    {
        cursor.mark(at, end, role.value());
    }
    return end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at, bool /*signature*/)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 5> marks{{{'"', lex_string},
                                                                {'\'', lex_string},
                                                                {'#', lex_comment},
                                                                {'@', lex_annotation},
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

// 行が `func` か `static func` で始まるか（引数の型の注釈を読む行）。
[[nodiscard]] constexpr bool declares_function(std::string_view line) noexcept
{
    const std::size_t first = lex_first_visible(line);
    std::size_t end = lex_word_end(line, first);
    std::string_view word = line.substr(first, end - first);
    if (word == "static")
    {
        std::size_t next = end;
        while (next < line.size() && lex_is_space(line[next]))
        {
            ++next;
        }
        end = lex_word_end(line, next);
        word = line.substr(next, end - next);
    }
    return word == "func";
}

// ---- 行をまたぐ状態の続き ----

// 3 連引用符の中で始まる行。閉じの 3 文字までが string で、閉じなければ行全体が string のまま。
std::size_t resume_triple(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::size_t end = triple_close(line, 0U, lex_delimiter(in));
    if (end == lex_unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, end, SyntaxRole::string);
    return end;
}

// GDScript にブロックコメントは無く、3 連引用符でない区切りは GDScript の状態でないので行頭から
// 読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    const std::string_view delimiter = lex_delimiter(in);
    switch (in.kind)
    {
    case LexKind::plain:
    case LexKind::block_comment:
        return 0U;
    case LexKind::raw_string:
        return delimiter == "\"\"\"" || delimiter == "'''" ? resume_triple(cursor, in) : 0U;
    }
    std::unreachable();
}
} // namespace

void lex_gdscript(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const bool signature = declares_function(line);
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at, signature);
    }
}
} // namespace nenenib::core
