#pragma once

#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// JSON と Python の行の字句器が共有するバイトの判定と走査（#830・ADR 0156 の追補）。どれも 1 行の
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
} // namespace nenenib::core
