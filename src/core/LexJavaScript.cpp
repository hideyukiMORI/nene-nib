#include "LexJavaScript.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexPrecedent.hpp"
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
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。定数・予約語・型の順に勝つ
// （`void` は予約語）。
constexpr std::array<std::string_view, 40> javascript_keywords{
    "async",    "await",    "break",  "case", "catch",  "class",  "const",      "continue",
    "debugger", "default",  "delete", "do",   "else",   "export", "extends",    "finally",
    "for",      "function", "get",    "if",   "import", "in",     "instanceof", "let",
    "new",      "of",       "return", "set",  "static", "super",  "switch",     "this",
    "throw",    "try",      "typeof", "var",  "void",   "while",  "with",       "yield"};
// TS の予約語（Language::typescript のときだけ引く・#833 の決定 5）。
constexpr std::array<std::string_view, 18> typescript_keywords{
    "abstract",  "as",        "declare", "enum",     "implements", "infer",
    "interface", "is",        "keyof",   "module",   "namespace",  "override",
    "private",   "protected", "public",  "readonly", "satisfies",  "type"};
constexpr std::array<std::string_view, 6> javascript_constants{"Infinity", "NaN",  "false",
                                                               "null",     "true", "undefined"};
constexpr std::array<std::string_view, 9> typescript_types{
    "any", "bigint", "boolean", "never", "number", "object", "string", "symbol", "unknown"};
// 次の `/` を正規表現にする予約語（#833 の決定 2）。
constexpr std::array<std::string_view, 7> expression_keywords{"case", "do",     "else",  "in",
                                                              "of",   "return", "typeof"};
static_assert(std::ranges::is_sorted(javascript_keywords));
static_assert(std::ranges::is_sorted(typescript_keywords));
static_assert(std::ranges::is_sorted(javascript_constants));
static_assert(std::ranges::is_sorted(typescript_types));
static_assert(std::ranges::is_sorted(expression_keywords));

// `/` は自分の関数で読む（正規表現・コメント・除算）ので、演算子の連なりは `/` で切る。
constexpr std::string_view operator_characters = "+-*%=<>!&|^~?:.,;()[]{}";

[[nodiscard]] constexpr bool is_identifier(char value) noexcept
{
    return lex_is_word(value) || value == '$';
}

[[nodiscard]] constexpr std::size_t identifier_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && is_identifier(line[at]))
    {
        ++at;
    }
    return at;
}

// from から needle を探し、その直後を返す。escaped なら `\` は次の 1 バイトを飛ばす。
[[nodiscard]] constexpr std::size_t closing_of(std::string_view line, std::size_t from,
                                               std::string_view needle, bool escaped) noexcept
{
    for (std::size_t at = from; at < line.size(); ++at)
    {
        if (line.substr(at).starts_with(needle))
        {
            return at + needle.size();
        }
        at += escaped && line[at] == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

[[nodiscard]] constexpr LexState template_state() noexcept
{
    return LexState{.kind = LexKind::raw_string, .delimiter = {'`'}, .delimiter_length = 1U};
}

// ---- コメント ----

std::size_t lex_line_comment(LexCursor &cursor, std::size_t at)
{
    const std::size_t end = cursor.line().size();
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

// begin から、from の後ろの `*/` までがコメント。閉じなければ行末までで次の行へ block_comment。
std::size_t mark_block_comment(LexCursor &cursor, std::size_t begin, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t end = closing_of(line, from, "*/", false);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::comment);
        cursor.leave(
            LexState{.kind = LexKind::block_comment, .delimiter = {}, .delimiter_length = 0U});
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::comment);
    return end;
}

// ---- 文字列・テンプレート・正規表現 ----

// `'…'` `"…"` は行をまたがず、閉じていなければ行末まで error。
std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_closing_quote(line, at);
    cursor.follow(LexPrecedent::operand);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::error);
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

// begin から、from の後ろの `` ` `` までがテンプレート。`${…}` の中も string のまま（後続）。
// 閉じなければ行末まで string で次の行へ raw_string（delimiter は `` ` ``）。
std::size_t mark_template(LexCursor &cursor, std::size_t begin, std::size_t from)
{
    const std::string_view line = cursor.line();
    const std::size_t end = closing_of(line, from, "`", true);
    cursor.follow(LexPrecedent::operand);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(template_state());
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

std::size_t lex_template(LexCursor &cursor, std::size_t at)
{
    return mark_template(cursor, at, at + 1U);
}

// at の `/` の後ろから正規表現の閉じの `/` を探し、その直後を返す。`\` は次の 1 バイトを飛ばし、
// 文字クラス `[…]` の中の `/` では閉じない。
[[nodiscard]] constexpr std::size_t regex_close(std::string_view line, std::size_t at) noexcept
{
    bool in_class = false;
    for (std::size_t index = at + 1U; index < line.size(); ++index)
    {
        const char value = line[index];
        if (value == '/' && !in_class)
        {
            return index + 1U;
        }
        in_class = value == '[' || (in_class && value != ']');
        index += value == '\\' ? 1U : 0U;
    }
    return lex_unclosed;
}

// 正規表現リテラル `/…/flags` は string。閉じが行末まで無ければ正規表現でなく、`/` を 1 字の
// 除算の演算子として読み直して続ける（#836 のレビュー R2・行頭のコメントの後や継続行の `/ 2`・
// JSX の `</div>` を error にしない）。
std::size_t mark_regex(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t close = regex_close(line, at);
    if (close == lex_unclosed)
    {
        cursor.mark(at, at + 1U, SyntaxRole::operators);
        cursor.follow(LexPrecedent::expression_start);
        return at + 1U;
    }
    cursor.follow(LexPrecedent::operand);
    const std::size_t end = lex_word_end(line, close);
    cursor.mark(at, end, SyntaxRole::string);
    return end;
}

// `/` はコメント・正規表現・除算（`/` か `/=`）のどれか。正規表現か除算かは直前の字句で決める。
std::size_t lex_slash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const char next = lex_next_of(line, at);
    if (next == '/')
    {
        return lex_line_comment(cursor, at);
    }
    if (next == '*')
    {
        return mark_block_comment(cursor, at, at + 2U);
    }
    if (cursor.precedent() == LexPrecedent::expression_start)
    {
        return mark_regex(cursor, at);
    }
    const std::size_t end = at + (next == '=' ? 2U : 1U);
    cursor.mark(at, end, SyntaxRole::operators);
    cursor.follow(LexPrecedent::expression_start);
    return end;
}

// ---- 数字と演算子 ----

[[nodiscard]] constexpr bool starts_number(std::string_view line, std::size_t at) noexcept
{
    return line[at] == '.' && lex_is_digit(lex_next_of(line, at));
}

// 数字の途中の 1 バイトが数字を続けるか。`_` `n` `x` `b` `o` は識別子の文字に含まれ、符号は 16 進
// でない数の指数の直後だけ。
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
    cursor.follow(LexPrecedent::operand);
    return end;
}

[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    return operator_characters.contains(line[at]) && !starts_number(line, at);
}

// 演算子の連なりは 1 区間（`.5` で切る）。最後の文字が `)` `]` なら次の `/` は除算、ほかは
// 正規表現。
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
    const char last = line[end - 1U];
    cursor.follow(last == ')' || last == ']' ? LexPrecedent::operand
                                             : LexPrecedent::expression_start);
    return end;
}

// ---- 識別子 ----

// at の識別子の直前が `.`（`...` を除く）なら、プロパティの名前で予約語・定数・型にしない。
[[nodiscard]] constexpr bool is_property(std::string_view line, std::size_t at) noexcept
{
    return at > 0U && line[at - 1U] == '.' && (at < 2U || line[at - 2U] != '.');
}

// 定数・予約語（TS では TS の予約語も）・型（TS だけ・JS に型の注釈は無い）の表の順に引き、
// どれでもなく直後が `(` なら関数。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool typescript,
                                                  bool called) noexcept
{
    if (std::ranges::binary_search(javascript_constants, word))
    {
        return SyntaxRole::constant;
    }
    if (std::ranges::binary_search(javascript_keywords, word) ||
        (typescript && std::ranges::binary_search(typescript_keywords, word)))
    {
        return SyntaxRole::keyword;
    }
    if (typescript && std::ranges::binary_search(typescript_types, word))
    {
        return SyntaxRole::type;
    }
    return called ? std::optional{SyntaxRole::function} : std::nullopt;
}

template <bool typescript> std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = identifier_end(line, at);
    const std::string_view word = line.substr(at, end - at);
    const bool called = end < line.size() && line[end] == '(';
    const std::optional<SyntaxRole> role =
        is_property(line, at) ? (called ? std::optional{SyntaxRole::function} : std::nullopt)
                              : word_role(word, typescript, called);
    if (role.has_value())
    {
        cursor.mark(at, end, role.value());
    }
    const bool opens =
        role == SyntaxRole::keyword && std::ranges::binary_search(expression_keywords, word);
    cursor.follow(opens ? LexPrecedent::expression_start : LexPrecedent::operand);
    return end;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 4> marks{
        {{'"', lex_string}, {'\'', lex_string}, {'`', lex_template}, {'/', lex_slash}}};
    for (const auto &[mark, handler] : marks)
    {
        if (value == mark)
        {
            return handler;
        }
    }
    return operator_characters.contains(value) ? lex_operators : skip_character;
}

template <bool typescript> [[nodiscard]] constexpr LexHandler handler_of(char value) noexcept
{
    if (lex_is_digit(value))
    {
        return lex_number;
    }
    return is_identifier(value) ? lex_word<typescript> : marked_handler(value);
}

// バイト → 字句の関数の表（ADR 0156 の決定 6）。JS と TS は識別子の関数だけが違う。空白・`#`
// `@` `\`・0x80 以上は skip_character。
template <bool typescript>
constexpr auto handlers = []
{
    std::array<LexHandler, 256> table{};
    for (std::size_t index = 0; index < table.size(); ++index)
    {
        table.at(index) = handler_of<typescript>(static_cast<char>(index));
    }
    return table;
}();

// ---- 行をまたぐ状態の続き ----

// ブロックコメントかテンプレートの中で始まる行は、閉じまでを読んでから続ける。区切りが `` ` `` で
// ない raw_string は JavaScript の状態でないので行頭から読む。
std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return mark_block_comment(cursor, 0U, 0U);
    case LexKind::raw_string:
        return lex_delimiter(in) == "`" ? mark_template(cursor, 0U, 0U) : 0U;
    }
    std::unreachable();
}

template <bool typescript> void lex_with(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers<typescript>.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace

void lex_javascript(LexCursor &cursor, const LexState &in, Language language)
{
    if (language == Language::typescript)
    {
        lex_with<true>(cursor, in);
        return;
    }
    lex_with<false>(cursor, in);
}
} // namespace nenenib::core
