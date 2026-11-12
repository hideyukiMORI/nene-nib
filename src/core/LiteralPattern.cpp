#include "LiteralPattern.hpp"

#include "DisplayWidth.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

namespace nenenib::core
{
namespace
{
// 語の先頭の結合文字（幅 0・ADR 0053 の表）。照合器は先頭の結合文字を結合文字だけの原子に読む
// （#1506）ので、literal のまま比べるには `\` で逃がす。2 つめからは literal の続きになる。
[[nodiscard]] bool leading_mark(std::string_view text) noexcept
{
    return !text.empty() && validate_utf8(text).has_value() &&
           display_width(code_point_at(text, Offset{0})) == DisplayWidth::zero;
}
} // namespace

std::string literal_pattern(std::string_view text, VimSearchDirection direction)
{
    constexpr std::string_view special = "\\/.*[~^$";
    const bool backward = direction == VimSearchDirection::backward;
    std::string escaped;
    escaped.reserve(text.size() + 1);
    if (leading_mark(text))
    {
        escaped.push_back('\\');
    }
    for (const char byte : text)
    {
        if (special.contains(byte) || (backward && byte == '?'))
        {
            escaped.push_back('\\');
        }
        escaped.push_back(byte);
    }
    return escaped;
}
} // namespace nenenib::core
