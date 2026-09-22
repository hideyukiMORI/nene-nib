#include "VirtualColumn.hpp"

#include "DisplayWidthRange.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// Vim の既定。設定にはしない（ADR 0034 の決定 4）。
constexpr std::size_t tab_stop = 8;
constexpr char32_t tab_character = U'\t';

[[nodiscard]] std::size_t cells_of(DisplayWidth width) noexcept
{
    switch (width)
    {
    case DisplayWidth::zero:
        return 0;
    case DisplayWidth::single:
        return 1;
    case DisplayWidth::wide:
        return 2;
    case DisplayWidth::unprintable:
        return 6;
    }
    std::unreachable();
}

// 桁 `column`（1 始まり）に置いた 1 文字が伸ばす桁の数。Tab だけが今いる桁に依る。
[[nodiscard]] std::size_t advance_of(char32_t value, std::size_t column) noexcept
{
    if (value == tab_character)
    {
        return tab_stop - ((column - 1) % tab_stop);
    }
    return cells_of(display_width(value));
}

// 行頭から `byte` の文字までの桁（1 始まり）＝その文字が占める最初の桁。
[[nodiscard]] std::size_t column_before(std::string_view line, std::size_t byte) noexcept
{
    std::size_t column = 1;
    std::size_t at = 0;
    while (at < byte && at < line.size())
    {
        column += advance_of(code_point_at(line, Offset{at}), column);
        at = next_code_point(line, Offset{at}).value;
    }
    return column;
}

// `byte` の文字が占める最後の桁。桁を増やさない文字は最初の桁と同じ。
[[nodiscard]] std::size_t column_through(std::string_view line, std::size_t byte) noexcept
{
    const std::size_t column = column_before(line, byte);
    if (byte >= line.size())
    {
        return column;
    }
    const std::size_t advance = advance_of(code_point_at(line, Offset{byte}), column);
    return advance == 0 ? column : column + advance - 1;
}

// `column` を含む文字の、行頭からのバイト位置。行がそれより短ければ行の内容の終わり
// （Vim が NUL を置く桁）で、`Column` で数えていたときの offset_of と同じ止まり方をする。
// NORMAL でそこから 1 文字ぶん戻すのも、VISUAL で改行まで選ぶのも呼ぶ側の仕事（ADR 0018 の決定
// 6）。
[[nodiscard]] std::size_t byte_at_column(std::string_view line, std::size_t column) noexcept
{
    std::size_t at = 0;
    std::size_t current = 1;
    while (at < line.size())
    {
        const std::size_t advance = advance_of(code_point_at(line, Offset{at}), current);
        if (advance != 0 && column < current + advance)
        {
            return at;
        }
        current += advance;
        at = next_code_point(line, Offset{at}).value;
    }
    return line.size();
}

// 行の内容と、その中での `at` のバイト位置。3 つの桁の関数が同じ 2 つを要る。
[[nodiscard]] std::pair<std::string, std::size_t> line_and_byte(const TextBuffer &text, Offset at)
{
    const LineNumber line = text.position_of(at).line;
    return {text.line_text(line), at.value - text.line_start(line).value};
}
} // namespace

DisplayWidth display_width(char32_t value) noexcept
{
    const auto found =
        std::ranges::lower_bound(display_width_ranges, value, {}, &DisplayWidthRange::last);
    if (found != display_width_ranges.end() && found->first <= value)
    {
        return found->width;
    }
    return DisplayWidth::single;
}

VirtualColumn virtual_column(const TextBuffer &text, Offset at)
{
    const auto [line, byte] = line_and_byte(text, at);
    return VirtualColumn{column_before(line, byte)};
}

VirtualColumn virtual_column_end(const TextBuffer &text, Offset at)
{
    const auto [line, byte] = line_and_byte(text, at);
    return VirtualColumn{column_through(line, byte)};
}

VirtualColumn caret_virtual_column(const TextBuffer &text, Offset at)
{
    const auto [line, byte] = line_and_byte(text, at);
    const bool on_tab = byte < line.size() && code_point_at(line, Offset{byte}) == tab_character;
    return VirtualColumn{on_tab ? column_through(line, byte) : column_before(line, byte)};
}

Offset offset_at_virtual_column(const TextBuffer &text, LineNumber line, VirtualColumn column)
{
    return Offset{text.line_start(line).value + byte_at_column(text.line_text(line), column.value)};
}
} // namespace nenenib::core
