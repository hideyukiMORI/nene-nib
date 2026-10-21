#pragma once

#include "LexKind.hpp"
#include "LexState.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

// kind に入って次の行へ渡す状態（delimiter は raw_delimiter_limit 文字まで）。
[[nodiscard]] constexpr LexState lex_entered(LexKind kind, std::string_view delimiter) noexcept
{
    LexState state{.kind = kind,
                   .delimiter = {},
                   .delimiter_length = static_cast<std::uint8_t>(delimiter.size())};
    std::ranges::copy(delimiter, state.delimiter.begin());
    return state;
}
} // namespace nenenib::core
