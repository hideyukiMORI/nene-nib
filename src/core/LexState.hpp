#pragma once

#include "LexKind.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nenenib::core
{
// raw string の区切り（`R"xyz(` の xyz）の最大の長さ。C++ の上限と同じで、超えた区切りは raw string
// として扱わず通常の文字列にする（#815 の限界）。
inline constexpr std::size_t raw_delimiter_limit = 16U;

// 行の終わりで次の行へ渡す字句の状態（#815・ADR 0156 の決定 3）。値だけで、既定は plain。
// delimiter は raw_string のときの区切りで、先頭の delimiter_length 文字だけが意味を持つ。
struct LexState
{
    LexKind kind = LexKind::plain;
    std::array<char, raw_delimiter_limit> delimiter{};
    std::uint8_t delimiter_length = 0U;
};

[[nodiscard]] constexpr std::string_view lex_delimiter(const LexState &state) noexcept
{
    return std::string_view{state.delimiter.data(), state.delimiter_length};
}

[[nodiscard]] constexpr bool operator==(const LexState &left, const LexState &right) noexcept
{
    return left.kind == right.kind && lex_delimiter(left) == lex_delimiter(right);
}
} // namespace nenenib::core
