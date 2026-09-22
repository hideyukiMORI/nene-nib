#include "DisplayLine.hpp"

#include "DisplayWidth.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr char32_t first_printable = U'\x20';
constexpr char32_t caret_offset = U'\x40';
constexpr std::string_view hex_digits = "0123456789abcdef";
constexpr std::size_t hex_width = 4;
constexpr unsigned hex_bits = 4;
constexpr char32_t hex_mask = 0xF;

// `^` と、制御文字に 0x40 を足した文字（`\r` → `^M`）。
[[nodiscard]] std::size_t append_caret_notation(std::string &out, char32_t value)
{
    out.push_back('^');
    out.push_back(static_cast<char>(value + caret_offset));
    return 2;
}

// `<` と小文字 16 進 4 桁と `>`（U+200B → `<200b>`）。表の書式用文字は U+FFFF 以下だけである。
[[nodiscard]] std::size_t append_hex_notation(std::string &out, char32_t value)
{
    out.push_back('<');
    for (std::size_t digit = hex_width; digit > 0; --digit)
    {
        const char32_t nibble = (value >> ((digit - 1) * hex_bits)) & hex_mask;
        out.push_back(hex_digits[static_cast<std::size_t>(nibble)]);
    }
    out.push_back('>');
    return hex_width + 2;
}

// 1 つの code point を描画の文字列へ足し、足した描画の code point 数を返す。
[[nodiscard]] std::size_t append_display(std::string &out, char32_t value)
{
    switch (display_width(value))
    {
    case DisplayWidth::wide:
        if (value < first_printable)
        {
            return append_caret_notation(out, value);
        }
        break;
    case DisplayWidth::unprintable:
        return append_hex_notation(out, value);
    case DisplayWidth::zero:
    case DisplayWidth::single:
        break;
    }
    append_utf8(out, value);
    return 1;
}
} // namespace

DisplayLine display_line(std::string_view text)
{
    DisplayLine line;
    line.text.reserve(text.size());
    line.starts.reserve(code_point_count(text) + 1);
    std::size_t position = 0;
    std::size_t at = 0;
    while (at < text.size())
    {
        line.starts.push_back(position);
        position += append_display(line.text, code_point_at(text, Offset{at}));
        at = next_code_point(text, Offset{at}).value;
    }
    line.starts.push_back(position);
    return line;
}

std::size_t display_position(const DisplayLine &line, std::size_t column) noexcept
{
    if (line.starts.empty())
    {
        return 0;
    }
    return line.starts[std::min(column, line.starts.size() - 1)];
}

std::size_t source_column(const DisplayLine &line, std::size_t position) noexcept
{
    if (line.starts.empty())
    {
        return 0;
    }
    // position 以下で最大の starts の添字。starts[0] は 0 なので必ず 1 つ以上ある。
    const auto after = std::upper_bound(line.starts.begin(), line.starts.end(), position);
    return static_cast<std::size_t>(std::distance(line.starts.begin(), after)) - 1;
}

bool is_replaced(const DisplayLine &line, std::size_t column) noexcept
{
    if (column + 1 >= line.starts.size())
    {
        return false;
    }
    return line.starts[column + 1] - line.starts[column] > 1;
}
} // namespace nenenib::core
