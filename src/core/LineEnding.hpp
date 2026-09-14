#pragma once

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
