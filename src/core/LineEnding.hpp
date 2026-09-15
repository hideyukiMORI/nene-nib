#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace nenenib::core
{
// 改行の形。読んだ形を保つのが既定で、新規の本文は CRLF（ARC-009 / ADR 0009 の決定 8）。
enum class LineEnding : std::uint8_t
{
    crlf,
    lf
};

[[nodiscard]] constexpr std::string_view newline_of(LineEnding ending) noexcept
{
    switch (ending)
    {
    case LineEnding::crlf:
        return "\r\n";
    case LineEnding::lf:
        return "\n";
    }
    std::unreachable();
}

// 読んだ本文の改行の形（ADR 0010 の決定 4）。最初の LF の直前が CR なら CRLF、
// そうでなければ LF、LF がまったく無ければ CRLF。本文のバイト列は変えない。
[[nodiscard]] constexpr LineEnding detect_line_ending(std::string_view text) noexcept
{
    const std::size_t first = text.find('\n');
    if (first == std::string_view::npos)
    {
        return LineEnding::crlf;
    }
    if (first > 0 && text[first - 1] == '\r')
    {
        return LineEnding::crlf;
    }
    return LineEnding::lf;
}

[[nodiscard]] constexpr std::string_view line_ending_label(LineEnding ending) noexcept
{
    switch (ending)
    {
    case LineEnding::crlf:
        return "CRLF";
    case LineEnding::lf:
        return "LF";
    }
    std::unreachable();
}
} // namespace nenenib::core
