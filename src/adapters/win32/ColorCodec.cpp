#include "ColorCodec.hpp"

#include <charconv>
#include <cstdint>

namespace nenenib::adapters::win32
{
namespace
{
[[nodiscard]] std::expected<std::uint32_t, application::ThemeFailure>
hex_value(std::string_view text, std::size_t digits)
{
    if (text.size() != digits + 1 || text.front() != '#')
    {
        return std::unexpected(application::ThemeFailure::invalid_color);
    }
    text.remove_prefix(1);
    std::uint32_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    {
        return std::unexpected(application::ThemeFailure::invalid_color);
    }
    return value;
}

[[nodiscard]] core::RgbColor rgb(std::uint32_t value) noexcept
{
    return core::RgbColor{static_cast<std::uint8_t>((value >> 16U) & 0xFFU),
                          static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
                          static_cast<std::uint8_t>(value & 0xFFU)};
}
} // namespace

std::expected<core::RgbColor, application::ThemeFailure> decode_rgb(std::string_view text)
{
    const auto value = hex_value(text, 6);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    return rgb(value.value());
}

std::expected<core::RgbaColor, application::ThemeFailure> decode_rgba(std::string_view text)
{
    const auto value = hex_value(text, 8);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    return core::RgbaColor{rgb(value.value() >> 8U),
                           static_cast<std::uint8_t>(value.value() & 0xFFU)};
}
} // namespace nenenib::adapters::win32
