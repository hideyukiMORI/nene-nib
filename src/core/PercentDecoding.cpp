#include "PercentDecoding.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
namespace
{
constexpr unsigned hex_radix = 16U;
constexpr unsigned decimal_digits = 10U;

// 16 進の 1 桁の値（16 進でなければ nullopt）。
[[nodiscard]] std::optional<unsigned> hex_digit(char value) noexcept
{
    if (value >= '0' && value <= '9')
    {
        return static_cast<unsigned>(value - '0');
    }
    if (value >= 'a' && value <= 'f')
    {
        return static_cast<unsigned>(value - 'a') + decimal_digits;
    }
    if (value >= 'A' && value <= 'F')
    {
        return static_cast<unsigned>(value - 'A') + decimal_digits;
    }
    return std::nullopt;
}
} // namespace

std::string percent_decoded(std::string_view text)
{
    std::string decoded;
    decoded.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const auto high = text[index] == '%' && index + 2U < text.size()
                              ? hex_digit(text[index + 1U])
                              : std::nullopt;
        const auto low = high.has_value() ? hex_digit(text[index + 2U]) : std::nullopt;
        if (low.has_value())
        {
            decoded.push_back(static_cast<char>((high.value() * hex_radix) + low.value()));
            index += 2U;
            continue;
        }
        decoded.push_back(text[index]);
    }
    return decoded;
}
} // namespace nenenib::core
