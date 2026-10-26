#pragma once

#include "AsciiCase.hpp"
#include "LexKind.hpp"
#include "LexState.hpp"
#include "LexWordGate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace nenenib::core
{
// JSON と Python の行の字句器が共有するバイトの判定と走査（#830・ADR 0156 の追補・#838 で
// HTML / XML・CSS・YAML・TOML も使う）。どれも 1 行の
// 本文のバイト列だけを見る純関数で、0x80 以上（UTF-8 の多バイト）は識別子にも空白にも数えない。
inline constexpr std::size_t lex_unclosed = std::string_view::npos;

[[nodiscard]] constexpr bool lex_is_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] constexpr bool lex_is_word(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           lex_is_digit(value) || value == '_';
}

[[nodiscard]] constexpr bool lex_is_space(char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\v' || value == '\f';
}

[[nodiscard]] constexpr char lex_next_of(std::string_view line, std::size_t at) noexcept
{
    return at + 1U < line.size() ? line[at + 1U] : '\0';
}

// at から識別子の文字が続く終わり。
[[nodiscard]] constexpr std::size_t lex_word_end(std::string_view line, std::size_t at) noexcept
{
    while (at < line.size() && lex_is_word(line[at]))
    {
        ++at;
    }
    return at;
}

// 行頭の空白の後の最初の位置。
[[nodiscard]] constexpr std::size_t lex_first_visible(std::string_view line) noexcept
{
    std::size_t at = 0;
    while (at < line.size() && lex_is_space(line[at]))
    {
        ++at;
    }
    return at;
}

// open の引用符の後ろから同じ引用符で閉じる所を探し、その直後を返す。`\` は次の 1 バイトを飛ばす。
// 閉じていなければ lex_unclosed。
[[nodiscard]] constexpr std::size_t lex_closing_quote(std::string_view line,
                                                      std::size_t open) noexcept
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
    return lex_unclosed;
}

// from から needle の最初の出現の位置。無ければ lex_unclosed。先頭の 1 バイトで探してから比べる
// （needle は数バイトの区切りで、1 行の中の探しは O(n × needle の長さ)）。
[[nodiscard]] constexpr std::size_t lex_find(std::string_view line, std::size_t from,
                                             std::string_view needle) noexcept
{
    for (std::size_t at = line.find(needle.front(), from); at != std::string_view::npos;
         at = line.find(needle.front(), at + 1U))
    {
        if (line.substr(at).starts_with(needle))
        {
            return at;
        }
    }
    return lex_unclosed;
}

// 大小を問わないキーワードの照合（#860・PHP・SQL・Dockerfile）。ASCII の A〜Z だけを畳んで
// 比べる順で、小文字だけの表はこの順でもソート済み（表の static_assert は
// std::ranges::is_sorted）。
[[nodiscard]] constexpr bool lex_folded_less(std::string_view left, std::string_view right) noexcept
{
    return std::ranges::lexicographical_compare(left, right, {}, ascii_folded, ascii_folded);
}

// ---- 表の門（#859） ----

// word の先頭バイト（gate.folded なら ASCII の小文字に畳む）と長さが門を通るか。
[[nodiscard]] constexpr bool lex_gate_admits(const LexWordGate &gate,
                                             std::string_view word) noexcept
{
    if (word.size() < gate.shortest || word.size() > gate.longest)
    {
        return false;
    }
    const unsigned char first =
        gate.folded ? ascii_folded(word.front()) : static_cast<unsigned char>(word.front());
    return ((gate.first.at(first / 64U) >> (first % 64U)) & 1U) != 0U;
}

// 表の全語から門を作る。表に空の語があればコンパイルが落ちる（front を読む）。
template <std::size_t count>
[[nodiscard]] consteval LexWordGate lex_gate_from(const std::array<std::string_view, count> &table,
                                                  bool folded)
{
    LexWordGate gate{.first = {}, .shortest = table.front().size(), .longest = 0, .folded = folded};
    for (const std::string_view word : table)
    {
        const unsigned char first =
            folded ? ascii_folded(word.front()) : static_cast<unsigned char>(word.front());
        gate.first.at(first / 64U) |= std::uint64_t{1} << (first % 64U);
        gate.shortest = std::min(gate.shortest, word.size());
        gate.longest = std::max(gate.longest, word.size());
    }
    return gate;
}

// 表の全語が門を通るか（偽陰性が無いことの確かめ）。
template <std::size_t count>
[[nodiscard]] consteval bool lex_gate_holds(const std::array<std::string_view, count> &table,
                                            const LexWordGate &gate)
{
    return std::ranges::all_of(table, [&gate](std::string_view word)
                               { return lex_gate_admits(gate, word); });
}

// 表ごとの門（表の参照ごとに 1 つ・コンパイル時に作る）。
template <const auto &table, bool folded>
inline constexpr LexWordGate lex_gate_of = lex_gate_from(table, folded);

// ASCII の順の表に word があるか。門で落ちる語は表を引かない。
template <const auto &table> [[nodiscard]] constexpr bool lex_in(std::string_view word) noexcept
{
    static_assert(lex_gate_holds(table, lex_gate_of<table, false>));
    return lex_gate_admits(lex_gate_of<table, false>, word) &&
           std::ranges::binary_search(table, word);
}

// 小文字の表に word が大小を問わずにあるか。門は先頭バイトを小文字に畳んで見る。
template <const auto &table>
[[nodiscard]] constexpr bool lex_folded_in(std::string_view word) noexcept
{
    static_assert(lex_gate_holds(table, lex_gate_of<table, true>));
    return lex_gate_admits(lex_gate_of<table, true>, word) &&
           std::ranges::binary_search(table, word, lex_folded_less);
}

// word が name（小文字）と大小を問わずに同じか（#862・Dockerfile の命令の名前と共用）。
[[nodiscard]] constexpr bool lex_folded_equal(std::string_view word, std::string_view name) noexcept
{
    return word.size() == name.size() && ascii_prefix_ignoring_case(word, name);
}

// kind に入って次の行へ渡す状態（delimiter は raw_delimiter_limit 文字まで）。
[[nodiscard]] constexpr LexState lex_entered(LexKind kind, std::string_view delimiter) noexcept
{
    LexState state{.kind = kind,
                   .delimiter = {},
                   .delimiter_length = static_cast<std::uint8_t>(delimiter.size())};
    std::ranges::copy(delimiter, state.delimiter.begin());
    return state;
}
// ---- 長い括弧（#860 の Lua・#862 の CMake） ----

// at の `[` から始まる長い括弧の開き `[=*[` の `=` の数。開きでないか、`=` が delimiter に
// 入りきらない（raw_delimiter_limit を越える）ときは nullopt。
[[nodiscard]] constexpr std::optional<std::size_t> lex_long_open(std::string_view line,
                                                                 std::size_t at) noexcept
{
    if (at >= line.size() || line[at] != '[')
    {
        return std::nullopt;
    }
    std::size_t end = at + 1U;
    while (end < line.size() && line[end] == '=')
    {
        ++end;
    }
    const std::size_t level = end - at - 1U;
    return end < line.size() && line[end] == '[' && level <= raw_delimiter_limit
               ? std::optional{level}
               : std::nullopt;
}

// from から同じ数の `=` の閉じ `]=*]` を探し、閉じの直後を返す。無ければ lex_unclosed。
[[nodiscard]] constexpr std::size_t lex_long_close(std::string_view line, std::size_t from,
                                                   std::size_t level) noexcept
{
    for (std::size_t at = line.find(']', from); at != lex_unclosed; at = line.find(']', at + 1U))
    {
        const std::size_t end = at + 1U + level;
        const bool equals = end < line.size() && std::ranges::all_of(
                                                     line.substr(at + 1U, level),
                                                     [](char value) { return value == '='; });
        if (equals && line[end] == ']')
        {
            return end + 1U;
        }
    }
    return lex_unclosed;
}

// 長い括弧の中（kind）で、delimiter に開きの `=` の列を持つ状態。
[[nodiscard]] constexpr LexState lex_long_state(LexKind kind, std::size_t level) noexcept
{
    constexpr std::string_view equals = "================";
    static_assert(equals.size() == raw_delimiter_limit);
    return lex_entered(kind, equals.substr(0U, level));
}

// delimiter が `=` だけの列（空も）か。長い括弧の中の状態かを見分ける。
[[nodiscard]] constexpr bool lex_long_level(std::string_view delimiter) noexcept
{
    return std::ranges::all_of(delimiter, [](char value) { return value == '='; });
}
} // namespace nenenib::core
