#include "QuantityText.hpp"

#include <array>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::size_t kibibyte = 1024U;

// 大きい単位から順に（割り切れる一番大きい単位を選ぶ）。行の型を足さず std::pair の別名で書く。
using SizeUnit = std::pair<std::size_t, std::string_view>;
constexpr std::array<SizeUnit, 3> size_units{{
    {kibibyte * kibibyte * kibibyte, "GiB"},
    {kibibyte * kibibyte, "MiB"},
    {kibibyte, "KiB"},
}};
} // namespace

std::string binary_size_text(std::size_t bytes)
{
    for (const auto &[size, name] : size_units)
    {
        if (bytes != 0 && bytes % size == 0)
        {
            return std::to_string(bytes / size) + " " + std::string(name);
        }
    }
    return std::to_string(bytes) + " bytes";
}

std::string grouped_decimal_text(std::size_t value)
{
    const std::string digits = std::to_string(value);
    std::string grouped;
    grouped.reserve(digits.size() + digits.size() / 3U);
    for (std::size_t index = 0; index < digits.size(); ++index)
    {
        if (index != 0 && (digits.size() - index) % 3U == 0)
        {
            grouped += ',';
        }
        grouped += digits[index];
    }
    return grouped;
}
} // namespace nenenib::core
