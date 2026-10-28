#include "JsonNumber.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::core
{
namespace
{
// 2^53。これより大きい整数は double で隣と区別できないので integer に持たない（ADR 0160）。
constexpr std::int64_t exact_integer_limit = std::int64_t{1} << 53;
constexpr double exact_double_limit = 9007199254740992.0;
} // namespace

JsonNumber json_number(double value) noexcept
{
    // NaN は両方の比較が偽なので外へ出る。libm の trunc は core の外へシンボルを出すので使わず、
    // 範囲の内で整数へ写して戻した値が同じかで整数かを決める（ARC-003）。
    if (!(value >= -exact_double_limit && value <= exact_double_limit))
    {
        return JsonNumber{.value = value, .integer = std::nullopt};
    }
    const auto whole = static_cast<std::int64_t>(value);
    if (static_cast<double>(whole) != value)
    {
        return JsonNumber{.value = value, .integer = std::nullopt};
    }
    return JsonNumber{.value = value, .integer = whole};
}

JsonNumber json_integer(std::int64_t value) noexcept
{
    if (value < -exact_integer_limit || value > exact_integer_limit)
    {
        return JsonNumber{.value = static_cast<double>(value), .integer = std::nullopt};
    }
    return JsonNumber{.value = static_cast<double>(value), .integer = value};
}

bool operator==(const JsonNumber &left, const JsonNumber &right) noexcept
{
    return left.value == right.value && left.integer == right.integer;
}
} // namespace nenenib::core
