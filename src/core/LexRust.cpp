#include "LexRust.hpp"

#include "LexCursor.hpp"
#include "LexKind.hpp"
#include "LexScan.hpp"
#include "LexState.hpp"
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
// 字句の関数。読み始めの位置を受けて、読み終えた次の位置を返す（必ず進む）。
using LexHandler = std::size_t (*)(LexCursor &, std::size_t);

// 表は ASCII の順に並べ、binary_search で引く（ADR 0156 の決定 6）。予約語は The Rust Reference
// の strict と reserved（Vim の rustKeyword と rustReservedKeyword）に、Vim が常に塗る union
// を足す。
constexpr std::array<std::string_view, 49> rust_keywords{
    "abstract", "as",       "async", "await", "become", "box",    "break",  "const",   "continue",
    "crate",    "do",       "dyn",   "else",  "enum",   "extern", "final",  "fn",      "for",
    "if",       "impl",     "in",    "let",   "loop",   "macro",  "match",  "mod",     "move",
    "mut",      "override", "priv",  "pub",   "ref",    "return", "self",   "static",  "struct",
    "super",    "trait",    "try",   "type",  "typeof", "union",  "unsafe", "unsized", "use",
    "virtual",  "where",    "while", "yield"};
// 組み込みの型と `Self`、Vim の rustTrait と rustEnum（rustType に結ばれる prelude の名前）。
constexpr std::array<std::string_view, 48> rust_types{"AsMut",
                                                      "AsRef",
                                                      "Box",
                                                      "Clone",
                                                      "Copy",
                                                      "Default",
                                                      "DoubleEndedIterator",
                                                      "Drop",
                                                      "Eq",
                                                      "ExactSizeIterator",
                                                      "Extend",
                                                      "Fn",
                                                      "FnMut",
                                                      "FnOnce",
                                                      "From",
                                                      "Into",
                                                      "IntoIterator",
                                                      "Iterator",
                                                      "Option",
                                                      "Ord",
                                                      "PartialEq",
                                                      "PartialOrd",
                                                      "Result",
                                                      "Self",
                                                      "Send",
                                                      "Sized",
                                                      "String",
                                                      "Sync",
                                                      "ToOwned",
                                                      "ToString",
                                                      "Vec",
                                                      "bool",
                                                      "char",
                                                      "f32",
                                                      "f64",
                                                      "i128",
                                                      "i16",
                                                      "i32",
                                                      "i64",
                                                      "i8",
                                                      "isize",
                                                      "str",
                                                      "u128",
                                                      "u16",
                                                      "u32",
                                                      "u64",
                                                      "u8",
                                                      "usize"};
// `true false` と Vim の rustEnumVariant（Constant に結ばれる）。
constexpr std::array<std::string_view, 6> rust_constants{"Err",  "None",  "Ok",
                                                         "Some", "false", "true"};
// 文字列の接頭辞（`b"` `c"`）と生の文字列の接頭辞（`r"` `br#"` `cr"`）。
constexpr std::array<std::string_view, 2> string_prefixes{"b", "c"};
constexpr std::array<std::string_view, 3> raw_prefixes{"br", "cr", "r"};
static_assert(std::ranges::is_sorted(rust_keywords));
static_assert(std::ranges::is_sorted(rust_types));
static_assert(std::ranges::is_sorted(rust_constants));
static_assert(std::ranges::is_sorted(string_prefixes));
static_assert(std::ranges::is_sorted(raw_prefixes));

constexpr std::string_view operator_characters = "+-*%=<>!&|^~?:.,;()[]{}@$";
// 通常の文字列の中（`\` は次の 1 バイトを飛ばす）の delimiter と、生の文字列の delimiter の頭。
constexpr std::string_view quote_mark = "\"";
constexpr char raw_mark = 'r';
// 文字の `'\u{10FFFF}'` の `\` から閉じの `'` までの最大のバイト数。
constexpr std::size_t escape_reach = 12U;
// 入れ子のコメントの深さの 10 進の桁の上限（delimiter の長さ）。越えた深さはここで止める。
constexpr std::uint64_t depth_limit = 9'999'999'999'999'999U;

[[nodiscard]] constexpr bool starts_name(char value) noexcept
{
    return lex_is_word(value) && !lex_is_digit(value);
}

// UTF-8 の先頭のバイトから、その code point のバイト数。
[[nodiscard]] constexpr std::size_t code_point_bytes(char lead) noexcept
{
    const auto byte = static_cast<unsigned char>(lead);
    if (byte < 0xC0U)
    {
        return 1U;
    }
    if (byte < 0xE0U)
    {
        return 2U;
    }
    return byte < 0xF0U ? 3U : 4U;
}

// ---- 入れ子のコメント ----

// 深さを 10 進で書いた block_comment の状態。
[[nodiscard]] LexState comment_state(std::uint64_t depth) noexcept
{
    std::array<char, raw_delimiter_limit> digits{};
    std::size_t first = digits.size();
    for (std::uint64_t rest = std::min(depth, depth_limit); rest > 0U; rest /= 10U)
    {
        --first;
        digits.at(first) = static_cast<char>('0' + (rest % 10U));
    }
    const std::string_view text{std::next(digits.data(), static_cast<std::ptrdiff_t>(first)),
                                digits.size() - first};
    return lex_entered(LexKind::block_comment, text);
}

// 状態の深さ。delimiter が空か桁でなければ 1（C / C++ と同じ形の状態から入った行）。
[[nodiscard]] constexpr std::uint64_t comment_depth(const LexState &in) noexcept
{
    const std::string_view digits = lex_delimiter(in);
    std::uint64_t depth = 0U;
    for (const char digit : digits)
    {
        if (!lex_is_digit(digit))
        {
            return 1U;
        }
        depth = (depth * 10U) + static_cast<std::uint64_t>(digit - '0');
    }
    return depth == 0U ? 1U : depth;
}

// at の 2 バイトが `/*` なら +1、`*/` なら −1、どちらでもなければ 0。
[[nodiscard]] constexpr int comment_turn(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    if (line[at] == '/' && next == '*')
    {
        return 1;
    }
    return line[at] == '*' && next == '/' ? -1 : 0;
}

// from から深さ depth の入れ子のコメントを読み、深さが 0 になった直後を返す（1 回の走査）。
// 閉じなければ lex_unclosed で、depth に残りの深さを置く。
[[nodiscard]] constexpr std::size_t nested_close(std::string_view line, std::size_t from,
                                                 std::uint64_t &depth) noexcept
{
    std::size_t at = from;
    while (at < line.size())
    {
        const int turn = comment_turn(line, at);
        at += turn == 0 ? 1U : 2U;
        depth = turn > 0 ? std::min(depth + 1U, depth_limit) : depth;
        depth -= turn < 0 ? 1U : 0U;
        if (depth == 0U)
        {
            return at;
        }
    }
    return lex_unclosed;
}

// at（コメントの始まり）から from を深さ depth で読んで塗る。閉じなければ行末まで comment で、
// 残りの深さを次の行へ。
std::size_t mark_nested(LexCursor &cursor, std::size_t at, std::size_t from, std::uint64_t depth)
{
    const std::string_view line = cursor.line();
    const std::size_t end = nested_close(line, from, depth);
    if (end == lex_unclosed)
    {
        cursor.mark(at, line.size(), SyntaxRole::comment);
        cursor.leave(comment_state(depth));
        return line.size();
    }
    cursor.mark(at, end, SyntaxRole::comment);
    return end;
}

// ---- 文字列 ----

// from から `\` を飛ばして `"` を探し、その直後を返す。行末の `\` は次の行へ続く。
[[nodiscard]] constexpr std::size_t escaped_close(std::string_view line, std::size_t from) noexcept
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

// begin（接頭辞の先頭）から open の `"` の文字列。閉じなければ行末まで string で次の行へ。
std::size_t mark_quoted(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::size_t end = escaped_close(line, open + 1U);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(lex_entered(LexKind::raw_string, quote_mark));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

std::size_t lex_string(LexCursor &cursor, std::size_t at)
{
    return mark_quoted(cursor, at, at);
}

// from から `#` が続く数。
[[nodiscard]] constexpr std::size_t hash_run(std::string_view line, std::size_t from) noexcept
{
    std::size_t end = from;
    while (end < line.size() && line[end] == '#')
    {
        ++end;
    }
    return end - from;
}

// 生の文字列の閉じ `"#…#`（hashes 個の `#`）を from から探し、その直後を返す。
[[nodiscard]] constexpr std::size_t raw_close(std::string_view line, std::size_t from,
                                              std::size_t hashes) noexcept
{
    for (std::size_t at = line.find('"', from); at != lex_unclosed; at = line.find('"', at + 1U))
    {
        if (hash_run(line, at + 1U) >= hashes)
        {
            return at + 1U + hashes;
        }
    }
    return lex_unclosed;
}

// 生の文字列の状態（delimiter は `r` と `#` の列）。
[[nodiscard]] LexState raw_state(std::string_view line, std::size_t hash, std::size_t hashes)
{
    std::array<char, raw_delimiter_limit> text{raw_mark};
    std::ranges::copy(line.substr(hash, hashes), std::next(text.begin()));
    return lex_entered(LexKind::raw_string, std::string_view{text.data(), hashes + 1U});
}

// begin（接頭辞の先頭）・hash（`#` の列の先頭）の生の文字列。`#` は 15 個までで、それより多ければ
// 生の文字列にしない（限界）。閉じなければ行末まで string で次の行へ。
std::size_t mark_raw(LexCursor &cursor, std::size_t begin, std::size_t hash)
{
    const std::string_view line = cursor.line();
    const std::size_t hashes = hash_run(line, hash);
    const std::size_t end = raw_close(line, hash + hashes + 1U, hashes);
    if (end == lex_unclosed)
    {
        cursor.mark(begin, line.size(), SyntaxRole::string);
        cursor.leave(raw_state(line, hash, hashes));
        return line.size();
    }
    cursor.mark(begin, end, SyntaxRole::string);
    return end;
}

// hash から `#` の列の後ろに `"` が来て、列が delimiter に入るか。
[[nodiscard]] constexpr bool opens_raw(std::string_view line, std::size_t hash) noexcept
{
    const std::size_t hashes = hash_run(line, hash);
    return hashes < raw_delimiter_limit && hash + hashes < line.size() &&
           line[hash + hashes] == '"';
}

// ---- 文字とライフタイム ----

// `\` の続きの文字 `'\n'` `'\''` `'\u{…}'`。escape は `\` の位置で、閉じの `'` は escape_reach
// バイトの中だけを探す。
[[nodiscard]] constexpr std::size_t escaped_character_end(std::string_view line,
                                                          std::size_t escape) noexcept
{
    const std::size_t last = std::min(line.size(), escape + escape_reach);
    for (std::size_t at = escape + 2U; at < last; ++at)
    {
        if (line[at] == '\'')
        {
            return at + 1U;
        }
    }
    return lex_unclosed;
}

// open の `'` が文字の始まりなら閉じの直後、そうでなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t character_end(std::string_view line, std::size_t open) noexcept
{
    const std::size_t body = open + 1U;
    if (body >= line.size())
    {
        return lex_unclosed;
    }
    if (line[body] == '\\')
    {
        return escaped_character_end(line, body);
    }
    const std::size_t close = body + code_point_bytes(line[body]);
    return close < line.size() && line[close] == '\'' ? close + 1U : lex_unclosed;
}

// begin（`b` の接頭辞の先頭）から open の `'`。文字なら string、`'name` はライフタイムかラベルで
// type、どちらでもなければ行末まで error。
std::size_t mark_apostrophe(LexCursor &cursor, std::size_t begin, std::size_t open)
{
    const std::string_view line = cursor.line();
    const std::size_t end = character_end(line, open);
    if (end != lex_unclosed)
    {
        cursor.mark(begin, end, SyntaxRole::string);
        return end;
    }
    if (begin == open && starts_name(lex_next_of(line, open)))
    {
        const std::size_t name = lex_word_end(line, open + 1U);
        cursor.mark(open, name, SyntaxRole::type);
        return name;
    }
    cursor.mark(begin, line.size(), SyntaxRole::error);
    return line.size();
}

std::size_t lex_apostrophe(LexCursor &cursor, std::size_t at)
{
    return mark_apostrophe(cursor, at, at);
}

// ---- 属性 ----

// 属性の中の at の 1 バイトで `[` `]` の深さを動かす。
[[nodiscard]] constexpr std::size_t bracket_depth(char value, std::size_t depth) noexcept
{
    if (value == '[')
    {
        return depth + 1U;
    }
    return value == ']' ? depth - 1U : depth;
}

// `#[` `#![` から対応する `]` までを keyword とし、中の文字列だけを string にする。閉じが行に
// 無ければ行末まで keyword（属性の行またぎは後続）。
std::size_t lex_attribute(LexCursor &cursor, std::size_t hash, std::size_t open)
{
    const std::string_view line = cursor.line();
    std::size_t depth = 0U;
    std::size_t segment = hash;
    for (std::size_t at = open; at < line.size();)
    {
        if (line[at] == '"')
        {
            cursor.mark(segment, at, SyntaxRole::keyword);
            at = mark_quoted(cursor, at, at);
            segment = at;
            continue;
        }
        depth = bracket_depth(line[at], depth);
        ++at;
        if (depth == 0U)
        {
            cursor.mark(segment, at, SyntaxRole::keyword);
            return at;
        }
    }
    cursor.mark(segment, line.size(), SyntaxRole::keyword);
    return line.size();
}

// `#[…]` `#![…]` は属性、ほかの `#` は演算子。
std::size_t lex_hash(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t open = at + (lex_next_of(line, at) == '!' ? 2U : 1U);
    if (open < line.size() && line[open] == '[')
    {
        return lex_attribute(cursor, at, open);
    }
    cursor.mark(at, at + 1U, SyntaxRole::operators);
    return at + 1U;
}

// ---- 演算子と数字 ----

[[nodiscard]] constexpr bool starts_comment(std::string_view line, std::size_t at) noexcept
{
    const char next = lex_next_of(line, at);
    return line[at] == '/' && (next == '/' || next == '*');
}

// 演算子の連なりを続けるか。コメントの始まりで切る。
[[nodiscard]] constexpr bool continues_operator(std::string_view line, std::size_t at) noexcept
{
    const bool slash = line[at] == '/' && !starts_comment(line, at);
    return slash || operator_characters.contains(line[at]);
}

// 数字の途中の 1 バイトが数字を続けるか。`_` と接尾辞（`u32` `f64`）は識別子の文字に含まれ、
// `.` は直後が桁のときだけ（`1..2` の範囲と `x.0.1` の手前で切る）、符号は 16 進でない数の指数の
// 直後だけ。
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
        cursor.mark(at, line.size(), SyntaxRole::comment); // `//` `///` `//!`
        return line.size();
    }
    if (lex_next_of(line, at) == '*')
    {
        return mark_nested(cursor, at, at + 2U, 1U);
    }
    return lex_operators(cursor, at);
}

// ---- 識別子 ----

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

// at の識別子の前が空白をはさんだ `fn` か（その名前は function）。
[[nodiscard]] constexpr bool follows_fn(std::string_view line, std::size_t at) noexcept
{
    const std::size_t end = skip_back_spaces(line, at);
    return end < at && end >= 2U && line.substr(end - 2U, 2U) == "fn" &&
           (end == 2U || !lex_is_word(line[end - 3U]));
}

// 識別子の直後が呼び出しの `(`・ターボフィッシュの `::<`・マクロの `!`（`!=` でない）か。
[[nodiscard]] constexpr std::size_t called_end(std::string_view line, std::size_t end) noexcept
{
    const std::string_view rest = line.substr(end);
    if (rest.starts_with('(') || rest.starts_with("::<"))
    {
        return end;
    }
    return rest.starts_with('!') && !rest.starts_with("!=") ? end + 1U : lex_unclosed;
}

// 識別子の後ろに引用符が続く接頭辞（`b'` `b"` `r#"` `br"`）なら字句を読み、そうでなければ
// lex_unclosed。
std::size_t prefixed_literal(LexCursor &cursor, std::size_t at, std::size_t end)
{
    const std::string_view line = cursor.line();
    const std::string_view word = line.substr(at, end - at);
    if (end >= line.size())
    {
        return lex_unclosed;
    }
    if (std::ranges::binary_search(raw_prefixes, word) && opens_raw(line, end))
    {
        return mark_raw(cursor, at, end);
    }
    if (line[end] == '\'' && word == "b")
    {
        return mark_apostrophe(cursor, at, end);
    }
    const bool quoted = line[end] == '"' && std::ranges::binary_search(string_prefixes, word);
    return quoted ? mark_quoted(cursor, at, end) : lex_unclosed;
}

// 予約語・型・定数の表の順に引き、どれでもなく `fn` の名前か呼び出しなら関数。
[[nodiscard]] std::optional<SyntaxRole> word_role(std::string_view word, bool named) noexcept
{
    if (std::ranges::binary_search(rust_keywords, word))
    {
        return SyntaxRole::keyword;
    }
    if (std::ranges::binary_search(rust_types, word))
    {
        return SyntaxRole::type;
    }
    if (std::ranges::binary_search(rust_constants, word))
    {
        return SyntaxRole::constant;
    }
    return named ? std::optional{SyntaxRole::function} : std::nullopt;
}

std::size_t lex_word(LexCursor &cursor, std::size_t at)
{
    const std::string_view line = cursor.line();
    const std::size_t end = lex_word_end(line, at);
    const std::size_t literal = prefixed_literal(cursor, at, end);
    if (literal != lex_unclosed)
    {
        return literal;
    }
    if (line.substr(at, end - at) == "r" && line.substr(end).starts_with('#'))
    {
        return lex_word_end(line, end + 1U); // 生の識別子 `r#match`（区間なし）
    }
    const std::size_t called = called_end(line, end);
    const std::optional<SyntaxRole> role =
        word_role(line.substr(at, end - at), called != lex_unclosed || follows_fn(line, at));
    if (!role.has_value())
    {
        return end;
    }
    // マクロの呼び出し `name!` は `!` までを function にする。
    const bool macro = role.value() == SyntaxRole::function && called == end + 1U;
    const std::size_t last = macro ? called : end;
    cursor.mark(at, last, role.value());
    return last;
}

std::size_t skip_character(LexCursor & /*cursor*/, std::size_t at)
{
    return at + 1U;
}

[[nodiscard]] constexpr LexHandler marked_handler(char value) noexcept
{
    constexpr std::array<std::pair<char, LexHandler>, 4> marks{
        {{'"', lex_string}, {'\'', lex_apostrophe}, {'/', lex_slash}, {'#', lex_hash}}};
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

// ---- 行をまたぐ状態の続き ----

// 文字列の中で始まる行。end が閉じの直後で、閉じなければ行全体が string のまま。
std::size_t resume_string(LexCursor &cursor, const LexState &in, std::size_t end)
{
    const std::string_view line = cursor.line();
    if (end == lex_unclosed)
    {
        cursor.mark(0U, line.size(), SyntaxRole::string);
        cursor.leave(in);
        return line.size();
    }
    cursor.mark(0U, end, SyntaxRole::string);
    return end;
}

// Rust の raw_string の区切り（`"` か `r` と `#` の列）でなければ Rust の状態でないので、行頭から
// 読む。
std::size_t resume_raw(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    const std::string_view delimiter = lex_delimiter(in);
    if (delimiter == quote_mark)
    {
        return resume_string(cursor, in, escaped_close(line, 0U));
    }
    const bool raw =
        delimiter.starts_with(raw_mark) && hash_run(delimiter, 1U) + 1U == delimiter.size();
    return raw ? resume_string(cursor, in, raw_close(line, 0U, delimiter.size() - 1U)) : 0U;
}

std::size_t resumed(LexCursor &cursor, const LexState &in)
{
    switch (in.kind)
    {
    case LexKind::plain:
        return 0U;
    case LexKind::block_comment:
        return mark_nested(cursor, 0U, 0U, comment_depth(in));
    case LexKind::raw_string:
        return resume_raw(cursor, in);
    }
    std::unreachable();
}
} // namespace

void lex_rust(LexCursor &cursor, const LexState &in)
{
    const std::string_view line = cursor.line();
    for (std::size_t at = resumed(cursor, in); at < line.size();)
    {
        at = handlers.at(static_cast<unsigned char>(line[at]))(cursor, at);
    }
}
} // namespace nenenib::core
