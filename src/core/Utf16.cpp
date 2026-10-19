#include "Utf16.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

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

constexpr std::size_t bytes_per_unit = 2;
constexpr unsigned int byte_shift = 8U;
constexpr char32_t byte_mask = 0xFF;

// bytes の index 番目の単位（2 バイト）を並びに従って読む。
[[nodiscard]] constexpr char32_t unit_of_bytes(std::string_view bytes, std::size_t index,
                                               ByteOrder order) noexcept
{
    const auto first = static_cast<char32_t>(static_cast<unsigned char>(bytes[index * 2]));
    const auto second = static_cast<char32_t>(static_cast<unsigned char>(bytes[(index * 2) + 1]));
    switch (order)
    {
    case ByteOrder::little:
        return first | (second << byte_shift);
    case ByteOrder::big:
        return (first << byte_shift) | second;
    }
    std::unreachable();
}

void append_unit_bytes(std::string &bytes, wchar_t unit, ByteOrder order)
{
    const auto value = static_cast<char32_t>(static_cast<std::uint16_t>(unit));
    const auto low = static_cast<char>(value & byte_mask);
    const auto high = static_cast<char>((value >> byte_shift) & byte_mask);
    switch (order)
    {
    case ByteOrder::little:
        bytes.push_back(low);
        bytes.push_back(high);
        return;
    case ByteOrder::big:
        bytes.push_back(high);
        bytes.push_back(low);
        return;
    }
    std::unreachable();
}

[[nodiscard]] constexpr bool high_surrogate(char32_t unit) noexcept
{
    return unit >= first_high_surrogate && unit < first_low_surrogate;
}

[[nodiscard]] constexpr bool low_surrogate(char32_t unit) noexcept
{
    return unit >= first_low_surrogate && unit <= last_low_surrogate;
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

bool valid_utf16_bytes(std::string_view bytes, ByteOrder order) noexcept
{
    if (bytes.size() % bytes_per_unit != 0)
    {
        return false;
    }
    const std::size_t units = bytes.size() / bytes_per_unit;
    std::size_t index = 0;
    while (index < units)
    {
        const char32_t unit = unit_of_bytes(bytes, index, order);
        ++index;
        if (low_surrogate(unit))
        {
            return false;
        }
        if (!high_surrogate(unit))
        {
            continue;
        }
        if (index >= units || !low_surrogate(unit_of_bytes(bytes, index, order)))
        {
            return false;
        }
        ++index;
    }
    return true;
}

std::expected<std::string, TextFailure> utf8_of_utf16_bytes(std::string_view bytes, ByteOrder order)
{
    if (bytes.size() % bytes_per_unit != 0)
    {
        return std::unexpected(TextFailure::invalid_utf16);
    }
    std::wstring units;
    units.reserve(bytes.size() / bytes_per_unit);
    for (std::size_t index = 0; index < bytes.size() / bytes_per_unit; ++index)
    {
        units.push_back(static_cast<wchar_t>(unit_of_bytes(bytes, index, order)));
    }
    return to_utf8(units);
}

std::expected<std::string, TextFailure> utf16_bytes_of_utf8(std::string_view utf8, ByteOrder order)
{
    const auto units = to_utf16(utf8);
    if (!units)
    {
        return std::unexpected(units.error());
    }
    std::string bytes;
    bytes.reserve(units.value().size() * bytes_per_unit);
    for (const wchar_t unit : units.value())
    {
        append_unit_bytes(bytes, unit, order);
    }
    return bytes;
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
