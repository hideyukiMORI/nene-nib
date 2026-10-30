#pragma once

#include "LineEnding.hpp"
#include "TextEncoding.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
// 保存の形の 1 つの選択（ADR 0010 の追補 #1049 の決定 2）。文字コードか改行の形のどちらかで、
// 入口（ステータスバーの面・`:set fenc=` `:set ff=`・F1 の操作）はどれもこの値を意図に載せる。
using DocumentFormat = std::variant<TextEncoding, LineEnding>;

// 面の候補の順（閉じた enum の全部・ADR 0010 の追補の決定 2）。値が増えたら下の名前の switch が
// 落ち、ここにも 1 つ足す。
inline constexpr std::array<TextEncoding, 5> text_encodings{
    TextEncoding::utf8, TextEncoding::utf8_bom, TextEncoding::shift_jis, TextEncoding::utf16_le,
    TextEncoding::utf16_be};
inline constexpr std::array<LineEnding, 2> line_endings{LineEnding::lf, LineEnding::crlf};

// Vim の 'fileencoding' の名前（`utf-8-bom` だけは Nib の名前で、Vim は 'bomb' を別に持つ）。
[[nodiscard]] constexpr std::string_view encoding_option_name(TextEncoding encoding) noexcept
{
    switch (encoding)
    {
    case TextEncoding::utf8:
        return "utf-8";
    case TextEncoding::utf8_bom:
        return "utf-8-bom";
    case TextEncoding::shift_jis:
        return "cp932";
    case TextEncoding::utf16_le:
        return "utf-16le";
    case TextEncoding::utf16_be:
        return "utf-16be";
    }
    std::unreachable();
}

// Vim の 'fileformat' の名前。
[[nodiscard]] constexpr std::string_view line_ending_option_name(LineEnding ending) noexcept
{
    switch (ending)
    {
    case LineEnding::lf:
        return "unix";
    case LineEnding::crlf:
        return "dos";
    }
    std::unreachable();
}

[[nodiscard]] constexpr std::optional<TextEncoding>
encoding_of_option_name(std::string_view name) noexcept
{
    for (const TextEncoding encoding : text_encodings)
    {
        if (encoding_option_name(encoding) == name)
        {
            return encoding;
        }
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<LineEnding>
line_ending_of_option_name(std::string_view name) noexcept
{
    for (const LineEnding ending : line_endings)
    {
        if (line_ending_option_name(ending) == name)
        {
            return ending;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::core
