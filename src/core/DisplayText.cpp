#include "DisplayText.hpp"

#include <array>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char32_t first_surrogate = 0xD800;
constexpr char32_t last_surrogate = 0xDFFF;
constexpr char32_t last_code_point = 0x10FFFF;

// UTF-8 の検証は自前で書く（ADR 0007）。<locale> / <codecvt> は使わない。
[[nodiscard]] constexpr std::size_t sequence_length(unsigned char lead) noexcept
{
    if (lead < 0x80U)
    {
        return 1;
    }
    if (lead >= 0xC0U && lead < 0xE0U)
    {
        return 2;
    }
    if (lead >= 0xE0U && lead < 0xF0U)
    {
        return 3;
    }
    if (lead >= 0xF0U && lead < 0xF8U)
    {
        return 4;
    }
    // 0x80-0xBF は孤立した継続バイト、0xF8 以降は UTF-8 に存在しない先頭バイト。
    return 0;
}

[[nodiscard]] constexpr char32_t lead_value(unsigned char lead, std::size_t length) noexcept
{
    constexpr std::array<unsigned char, 5> mask{0U, 0x7FU, 0x1FU, 0x0FU, 0x07U};
    return static_cast<char32_t>(lead & mask[length]);
}

[[nodiscard]] constexpr bool continuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

// 過長符号化・サロゲート範囲・U+10FFFF 超をまとめて拒否する。
[[nodiscard]] constexpr bool encodable(char32_t value, std::size_t length) noexcept
{
    constexpr std::array<char32_t, 5> smallest{0U, 0U, 0x80U, 0x800U, 0x10000U};
    return value >= smallest[length] && value <= last_code_point &&
           (value < first_surrogate || value > last_surrogate);
}

[[nodiscard]] constexpr bool control_character(char32_t value) noexcept
{
    return value < 0x20U || value == 0x7FU;
}

[[nodiscard]] std::expected<std::size_t, TextFailure> scan(std::string_view text,
                                                           std::size_t index) noexcept
{
    const auto lead = static_cast<unsigned char>(text[index]);
    const std::size_t length = sequence_length(lead);
    if (length == 0 || text.size() - index < length)
    {
        return std::unexpected(TextFailure::invalid_utf8);
    }
    char32_t value = lead_value(lead, length);
    for (std::size_t offset = 1; offset < length; ++offset)
    {
        const auto byte = static_cast<unsigned char>(text[index + offset]);
        if (!continuation(byte))
        {
            return std::unexpected(TextFailure::invalid_utf8);
        }
        value = (value << 6U) | static_cast<char32_t>(byte & 0x3FU);
    }
    if (!encodable(value, length))
    {
        return std::unexpected(TextFailure::invalid_utf8);
    }
    if (control_character(value))
    {
        return std::unexpected(TextFailure::control_character);
    }
    return length;
}
} // namespace

DisplayText::DisplayText(std::string text, std::size_t code_point_count)
    : text_(std::move(text)), code_point_count_(code_point_count)
{
}

std::expected<DisplayText, TextFailure> DisplayText::parse(std::string_view text)
{
    if (text.empty())
    {
        return std::unexpected(TextFailure::empty);
    }
    if (text.size() > maximum_bytes)
    {
        return std::unexpected(TextFailure::too_long);
    }
    std::size_t index = 0;
    std::size_t code_points = 0;
    while (index < text.size())
    {
        const auto length = scan(text, index);
        if (!length)
        {
            return std::unexpected(length.error());
        }
        index += *length;
        ++code_points;
    }
    return DisplayText(std::string(text), code_points);
}

std::string_view DisplayText::text() const noexcept
{
    return text_;
}

std::size_t DisplayText::code_point_count() const noexcept
{
    return code_point_count_;
}
} // namespace nenenib::core
