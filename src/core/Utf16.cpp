#include "Utf16.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
namespace
{
constexpr char32_t first_high_surrogate = 0xD800;
constexpr char32_t first_low_surrogate = 0xDC00;
constexpr char32_t last_low_surrogate = 0xDFFF;
constexpr char32_t first_supplementary = 0x10000;
constexpr unsigned int surrogate_shift = 10U;
constexpr char32_t low_surrogate_mask = 0x3FF;

[[nodiscard]] constexpr std::size_t scalar_units(char32_t value) noexcept
{
    return value < first_supplementary ? 1 : 2;
}

// 検証済みの code point を UTF-16 の 1〜2 単位へ。U+10000 以上だけがサロゲートペアになる。
void append_utf16(std::wstring &utf16, char32_t value)
{
    if (scalar_units(value) == 1)
    {
        utf16.push_back(static_cast<wchar_t>(value));
        return;
    }
    const char32_t rest = value - first_supplementary;
    utf16.push_back(static_cast<wchar_t>(first_high_surrogate + (rest >> surrogate_shift)));
    utf16.push_back(static_cast<wchar_t>(first_low_surrogate + (rest & low_surrogate_mask)));
}

[[nodiscard]] constexpr char32_t unit_at(std::wstring_view utf16, std::size_t index) noexcept
{
    return static_cast<char32_t>(static_cast<std::uint16_t>(utf16[index]));
}

// index の 1 単位（サロゲートペアなら 2 単位）を読み、index を読んだ分だけ進める。
// 対になっていないサロゲートは失敗で、index は進んだ位置のまま（呼び出し側は打ち切る）。
[[nodiscard]] std::expected<char32_t, TextFailure> take_scalar(std::wstring_view utf16,
                                                               std::size_t &index) noexcept
{
    const char32_t unit = unit_at(utf16, index);
    ++index;
    if (unit < first_high_surrogate || unit > last_low_surrogate)
    {
        return unit;
    }
    if (unit >= first_low_surrogate || index >= utf16.size())
    {
        return std::unexpected(TextFailure::invalid_utf16);
    }
    const char32_t low = unit_at(utf16, index);
    if (low < first_low_surrogate || low > last_low_surrogate)
    {
        return std::unexpected(TextFailure::invalid_utf16);
    }
    ++index;
    return first_supplementary + ((unit - first_high_surrogate) << surrogate_shift) +
           (low - first_low_surrogate);
}
} // namespace

std::expected<std::wstring, TextFailure> to_utf16(std::string_view utf8)
{
    const auto code_points = validate_utf8(utf8);
    if (!code_points)
    {
        return std::unexpected(code_points.error());
    }
    std::wstring utf16;
    utf16.reserve(code_points.value());
    std::size_t index = 0;
    while (index < utf8.size())
    {
        append_utf16(utf16, code_point_at(utf8, Offset{index}));
        index = next_code_point(utf8, Offset{index}).value;
    }
    return utf16;
}

std::expected<std::string, TextFailure> to_utf8(std::wstring_view utf16)
{
    std::string utf8;
    constexpr std::size_t maximum_bytes_per_unit = 3;
    const auto reserved = utf16.size() <= utf8.max_size() / maximum_bytes_per_unit
                              ? utf16.size() * maximum_bytes_per_unit
                              : utf16.size();
    utf8.reserve(reserved);
    std::size_t index = 0;
    while (index < utf16.size())
    {
        const auto scalar = take_scalar(utf16, index);
        if (!scalar)
        {
            return std::unexpected(scalar.error());
        }
        append_utf8(utf8, scalar.value());
    }
    return utf8;
}

std::expected<std::size_t, TextFailure> utf16_length(std::string_view utf8)
{
    const auto validated = validate_utf8(utf8);
    if (!validated)
    {
        return std::unexpected(validated.error());
    }
    std::size_t units = 0;
    for (std::size_t at = 0; at < utf8.size(); at = next_code_point(utf8, Offset{at}).value)
    {
        units += scalar_units(code_point_at(utf8, Offset{at}));
    }
    return units;
}
} // namespace nenenib::core
