#include "LexCpp.hpp"

#include "LexCharacter.hpp"
#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "SyntaxRole.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 種類ごとの字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

constexpr std::size_t unclosed = std::string_view::npos;

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。
constexpr std::array<std::string_view, 76> cpp_keywords{
    "_Alignas",      "_Alignof", "_Atomic",    "_Generic",  "_Noreturn", "_Static_assert",
    "_Thread_local", "alignas",  "alignof",    "asm",       "auto",      "break",
    "case",          "catch",    "class",      "co_await",  "co_return", "co_yield",
    "concept",       "const",    "const_cast", "consteval", "constexpr", "constinit",
    "continue",      "decltype", "default",    "delete",    "do",        "dynamic_cast",
    "else",          "enum",     "explicit",   "export",    "extern",    "final",
    "for",           "friend",   "goto",       "if",        "import",    "inline",
    "module",        "mutable",  "namespace",  "new",       "noexcept",  "operator",
    "override",      "private",  "protected",  "public",    "register",  "reinterpret_cast",
    "requires",      "restrict", "return",     "sizeof",    "static",    "static_assert",
    "static_cast",   "struct",   "switch",     "template",  "this",      "thread_local",
    "throw",         "try",      "typedef",    "typeid",    "typename",  "union",
    "using",         "virtual",  "volatile",   "while"};
// 組み込みと標準の基本型。`std::` の後ろでも同じ表で引く（std:: だけの型は後続）。
constexpr std::array<std::string_view, 34> cpp_types{
    "_Bool",   "_Complex",  "_Imaginary", "bool",     "char",        "char16_t",  "char32_t",
    "char8_t", "double",    "float",      "int",      "int16_t",     "int32_t",   "int64_t",
    "int8_t",  "intmax_t",  "intptr_t",   "long",     "max_align_t", "nullptr_t", "ptrdiff_t",
    "short",   "signed",    "size_t",     "ssize_t",  "uint16_t",    "uint32_t",  "uint64_t",
    "uint8_t", "uintmax_t", "uintptr_t",  "unsigned", "void",        "wchar_t"};
constexpr std::array<std::string_view, 4> cpp_constants{"NULL", "false", "nullptr", "true"};
// 文字列の接頭辞（`L"` `u8'`）と raw string の接頭辞（`R"` `LR"`）。
constexpr std::array<std::string_view, 4> string_prefixes{"L", "U", "u", "u8"};
constexpr std::array<std::string_view, 5> raw_prefixes{"LR", "R", "UR", "u8R", "uR"};
static_assert(std::ranges::is_sorted(cpp_keywords));
static_assert(std::ranges::is_sorted(cpp_types));
static_assert(std::ranges::is_sorted(cpp_constants));
static_assert(std::ranges::is_sorted(string_prefixes));
static_assert(std::ranges::is_sorted(raw_prefixes));

constexpr std::string_view operator_characters = "+-*%=<>!&|^~?:.,;()[]{}";

[[nodiscard]] constexpr bool is_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] constexpr bool is_word(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || is_digit(value) ||
           value == '_';
}

[[nodiscard]] constexpr bool is_space(char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\v' || value == '\f';
}

[[nodiscard]] constexpr LexCharacter marked_character(char value) noexcept
{
    constexpr std::array<std::pair<char, LexCharacter>, 4> marks{{{'"', LexCharacter::quote},
                                                                  {'\'', LexCharacter::apostrophe},
                                                                  {'/', LexCharacter::slash},
                                                                  {'#', LexCharacter::hash}}};
    for (const auto &[mark, character] : marks)
    {
        if (value == mark)
        {
            return character;
        }
    }
    return operator_characters.contains(value) ? LexCharacter::operators : LexCharacter::other;
}

[[nodiscard]] constexpr LexCharacter character_of(char value) noexcept
{
    if (is_space(value))
    {
        return LexCharacter::space;
    }
    if (is_digit(value))
    {
        return LexCharacter::digit;
    }
    return is_word(value) ? LexCharacter::word : marked_character(value);
}

constexpr auto character_table = []
{
    std::array<LexCharacter, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = character_of(static_cast<char>(index));
    }
    return table;
}();

[[nodiscard]] constexpr LexCharacter character_at(std::string_view line, std::size_t at) noexcept
{
    return character_table.at(static_cast<unsigned char>(line[at]));
}

[[nodiscard]] constexpr char next_of(std::string_view line, std::size_t at) noexcept
{
    return at + 1U < line.size() ? line[at + 1U] : '\0';
}

// ---- 文字列 ----

// open の引用符の後ろから閉じる引用符を探し、その直後を返す。`\` は次の 1 バイトを飛ばす。
[[nodiscard]] std::size_t closing_quote(std::string_view line, std::size_t open) noexcept
{
    const char quote = line[open];
    for (std::size_t at = open + 1U; at < line.size(); ++at)
    {
        if (line[at] == quote)
        {
            return at + 1U;
        }
        at += line[at] == '\\' ? 1U : 0U;
    }
    return unclosed;
}

// begin（接頭辞の先頭）から open の引用符で始まる文字列か文字リテラル。閉じていなければ行末まで
// error。
std::size_t mark_quoted(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::size_t end = closing_quote(line, open);
    if (end == unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::error);
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

std::size_t lex_quoted(LexCursor &cursor, std::size_t at)
{
    return mark_quoted(cursor, at, at);
}

[[nodiscard]] constexpr bool is_raw_delimiter_character(char value) noexcept
{
    return value > ' ' && value < '\x7f' && value != ')' && value != '\\';
}

// from から `(` までの区切りの長さ。16 文字を超える・使えない文字・`(` が無ければ unclosed。
[[nodiscard]] std::size_t raw_delimiter_length(std::string_view line, std::size_t from) noexcept
{
    const std::size_t last = std::min(line.size(), from + raw_delimiter_limit + 1U);
    for (std::size_t at = from; at < last; ++at)
    {
        if (line[at] == '(')
        {
            return at - from;
        }
        if (!is_raw_delimiter_character(line[at]))
        {
            return unclosed;
        }
    }
    return unclosed;
}

// from から `)delimiter"` を探し、その直後を返す。
[[nodiscard]] std::size_t raw_close(std::string_view line, std::size_t from,
                                    std::string_view delimiter) noexcept
{
    for (std::size_t at = line.find(')', from); at != unclosed; at = line.find(')', at + 1U))
    {
        const std::string_view tail = line.substr(at + 1U);
        if (tail.size() > delimiter.size() && tail.starts_with(delimiter) &&
            tail[delimiter.size()] == '"')
        {
            return at + delimiter.size() + 2U;
        }
    }
    return unclosed;
}

[[nodiscard]] LexState raw_state(std::string_view delimiter) noexcept
{
    LexState state{.kind = LexKind::raw_string,
                   .delimiter = {},
                   .delimiter_length = static_cast<std::uint8_t>(delimiter.size())};
    std::ranges::copy(delimiter, state.delimiter.begin());
    return state;
}

// begin（接頭辞の先頭）・quote（`"` の位置）の raw string。閉じていなければ行末まで string
// で次の行へ。
std::size_t lex_raw(LexCursor &cursor, std::size_t begin, std::size_t quote)
{
    const std::string_view line = cursor.line();
    const std::size_t length = raw_delimiter_length(line, quote + 1U);
    if (length == unclosed)
    {
        return mark_quoted(cursor, begin, quote);
    }
    const std::string_view delimiter = line.substr(quote + 1U, length);
    const std::size_t end = raw_close(line, quote + length + 2U, delimiter);
    if (end == unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(raw_state(delimiter));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

// ---- コメント ----

// from から `*/` を探し、`*` の位置を返す（1 文字の検索だけを使う・複数文字の find は core の外へ
// __std_search_1 を出して ARC-003 が落とす）。
[[nodiscard]] std::size_t comment_close(std::string_view line, std::size_t from) noexcept
{
    for (std::size_t at = line.find('*', from); at != unclosed; at = line.find('*', at + 1U))
    {
        if (at + 1U < line.size() && line[at + 1U] == '/')
        {
            return at;
        }
    }
    return unclosed;
}

constexpr LexState in_block_comment{
    .kind = LexKind::block_comment, .delimiter = {}, .delimiter_length = 0U};

std::size_t lex_block_comment(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close = comment_close(line, at + 2U);
    if (close == unclosed)
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
    const char next = next_of(line, at);
    return line[at] == '/' && (next == '/' || next == '*');
}

// 演算子の連なりを続けるか。コメントの始まりと、数字の始まりの `.`（`.5`）で切る。
[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    const LexCharacter character = character_at(line, at);
    if (character == LexCharacter::slash)
    {
        return !starts_comment(line, at);
    }
    return character == LexCharacter::operators &&
           !(line[at] == '.' && is_digit(next_of(line, at)));
}

[[nodiscard]] constexpr bool is_hex_digit(char value) noexcept
{
    const char lower = static_cast<char>(value | '\x20');
    return is_digit(value) || (lower >= 'a' && lower <= 'f');
}

// 数字の途中の 1 バイトが数字を続けるか。`'` は直後が桁のときだけ桁区切り、符号は指数の直後だけ。
[[nodiscard]] constexpr bool continues_number(std::string_view line, std::size_t at,
                                              bool hex) noexcept
{
    const char value = line[at];
    if (is_word(value) || value == '.')
    {
        return true;
    }
    if (value == '\'')
    {
        const char next = next_of(line, at);
        return hex ? is_hex_digit(next) : is_digit(next);
    }
    const char exponent = static_cast<char>(line[at - 1U] | '\x20');
    return (value == '+' || value == '-') && exponent == (hex ? 'p' : 'e');
}

std::size_t lex_number(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const bool hex = line[at] == '0' && (next_of(line, at) | '\x20') == 'x';
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
    if (line[at] == '.' && is_digit(next_of(line, at)))
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
    if (next_of(line, at) == '/')
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        return line.size();
    }
    if (next_of(line, at) == '*')
    {
        return lex_block_comment(cursor, at);
    }
    return lex_operators(cursor, at);
}

// ---- 識別子 ----

[[nodiscard]] std::size_t word_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && is_word(line[at]))
    {
        ++at;
    }
    return at;
}

// 識別子の後ろに引用符が続く接頭辞（`L"` `u8'` `R"(`）なら字句を読み、そうでなければ unclosed。
std::size_t prefixed_literal(LexCursor &cursor, std::size_t at, std::size_t end)
{
    const std::string_view line = cursor.line();
    if (end >= line.size())
    {
        return unclosed;
    }
    const std::string_view word = line.substr(at, end - at);
    if (line[end] == '"' && std::ranges::binary_search(raw_prefixes, word))
    {
        return lex_raw(cursor, at, end);
    }
    const bool quoted = line[end] == '"' || line[end] == '\'';
    if (quoted && std::ranges::binary_search(string_prefixes, word))
    {
        return mark_quoted(cursor, at, end);
    }
    return unclosed;
}

// 予約語・型・定数の表の順に引き、どれでもなく直後が `(` なら関数。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool called) noexcept
{
    if (std::ranges::binary_search(cpp_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (std::ranges::binary_search(cpp_types, word))
    {
        return SyntaxRole::type;
    }
    if (std::ranges::binary_search(cpp_constants, word))
    {
        return SyntaxRole::constant;
    }
    return called ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = word_end(line, at);
    const std::size_t literal = prefixed_literal(cursor, at, end);
    if (literal != unclosed)
    {
        return literal;
    }
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

// LexCharacter の宣言の順に並ぶ。行の途中の `#` は分類しない。
constexpr std::array<LexHandler, 9> handlers{skip_character, lex_word,      lex_number,
                                             lex_quoted,     lex_quoted,    lex_slash,
                                             skip_character, lex_operators, skip_character};
static_assert(handlers.size() == static_cast<std::size_t>(LexCharacter::other) + 1U);

// ---- プリプロセッサの行 ----

[[nodiscard]] std::size_t first_visible(std::string_view line) noexcept
{
    std::size_t at = 0;
    while (at < line.size() && is_space(line[at]))
    {
        ++at;
    }
    return at;
}

[[nodiscard]] bool includes_header(std::string_view line, std::size_t hash) noexcept
{
    const std::size_t name = first_visible(line.substr(hash + 1U)) + hash + 1U;
    return line.substr(name, word_end(line, name) - name) == "include";
}

// 前後の空白を除いた [begin, end) を keyword で塗る。
void mark_directive(LexCursor &cursor, std::size_t begin, std::size_t end)
{
    const std::string_view line = cursor.line();
    while (begin < end && is_space(line[begin]))
    {
        ++begin;
    }
    while (end > begin && is_space(line[end - 1U]))
    {
        --end;
    }
    cursor.mark(begin, end, SyntaxRole::keyword);
}

[[nodiscard]] bool starts_header(std::string_view line, std::size_t at, bool include) noexcept
{
    return include && line[at] == '<' && line.find('>', at + 1U) != unclosed;
}

// 指令の行の `'` は閉じるときだけ文字リテラル。閉じない `'`（`#error can't build`）は keyword の
// まま（Vim の c.vim も完結した形にしか当たらない・#820 のレビュー R1）。
[[nodiscard]] bool starts_character(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '\'' && closing_quote(line, at) != unclosed;
}

// プリプロセッサの行の中で keyword にしない字句（文字列・コメント・`#include` の
// `<…>`）が始まるか。
[[nodiscard]] bool starts_piece(std::string_view line, std::size_t at, bool include) noexcept
{
    return line[at] == '"' || starts_character(line, at) || starts_comment(line, at) ||
           starts_header(line, at, include);
}

std::size_t lex_piece(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    if (line[at] == '<')
    {
        const std::size_t end = line.find('>', at + 1U) + 1U;
        cursor.mark(at, end, SyntaxRole::string);
        return end;
    }
    return line[at] == '/' ? lex_slash(cursor, at) : lex_quoted(cursor, at);
}

// `#` から行末までを keyword とし、中の文字列・コメント・`<…>` だけを分ける（識別子は分類しない）。
void lex_directive(LexCursor &cursor, std::size_t hash)
{
    const std::string_view line = cursor.line();
    const bool include = includes_header(line, hash);
    std::size_t segment = hash;
    std::size_t at = hash + 1U;
    while (at < line.size())
    {
        if (!starts_piece(line, at, include))
        {
            ++at;
            continue;
        }
        mark_directive(cursor, segment, at);
        at = lex_piece(cursor, at);
        segment = at;
    }
    mark_directive(cursor, segment, line.size());
}

// ---- 行をまたぐ状態の続き ----

std::size_t resume_comment(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::size_t close = comment_close(line, 0U);
    if (close == unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::comment);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, close + 2U, SyntaxRole::comment);
    return close + 2U;
}

std::size_t resume_raw(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::size_t end = raw_close(line, 0U, lex_delimiter(in));
    if (end == unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, end, SyntaxRole::string);
    return end;
}

std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return resume_comment(cursor, in);
    case LexKind::raw_string:
        return resume_raw(cursor, in);
    }
    std::unreachable();
}

[[nodiscard]] bool starts_directive(const LexState &in, std::string_view line) noexcept
{
    const std::size_t hash = first_visible(line);
    return in.kind == LexKind::plain && hash < line.size() && line[hash] == '#';
}
} // namespace

void lex_cpp(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    if (starts_directive(in, line))
    {
        lex_directive(cursor, first_visible(line));
        return;
    }
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<std::size_t>(character_at(line, at)))(cursor, at);
    }
}
} // namespace nenenib::core
