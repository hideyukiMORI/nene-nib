#include "FontSize.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

namespace nenenib::core
{
FontSize::FontSize(float points) : points_(points) {}

std::expected<FontSize, FontSizeFailure> FontSize::parse(std::string_view text)
{
    if (text.empty())
    {
        return std::unexpected(FontSizeFailure::invalid_text);
    }
    float points = 0.0F;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), points);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    {
        return std::unexpected(FontSizeFailure::invalid_text);
    }
    return from_points(points);
}

std::expected<FontSize, FontSizeFailure> FontSize::from_points(float points)
{
    // 比較を正の条件で書き、NaN も拒否する。libm や既定ロケールを読まない。
    if (!(points >= minimum_points && points <= maximum_points))
    {
        return std::unexpected(FontSizeFailure::out_of_range);
    }
    return FontSize(points);
}

float FontSize::points() const noexcept
{
    return points_;
}

FontSize default_font_size()
{
    return FontSize::from_points(FontSize::default_points).value();
}

namespace
{
// 1 段は 0.5 pt（#1927）。0.5 pt を 1 とする整数で数え、端で止める（下限 8・上限 40 はどちらも
// 格子の上）。格子の外の値（`:set fontsize=13.3`）は、上げると上の格子、下げると下の格子へ着く。
constexpr std::int64_t half_points_per_point = 2;
constexpr std::int64_t lowest_half_points =
    static_cast<std::int64_t>(FontSize::minimum_points) * half_points_per_point;
constexpr std::int64_t highest_half_points =
    static_cast<std::int64_t>(FontSize::maximum_points) * half_points_per_point;
constexpr auto widest_walk = static_cast<std::size_t>(highest_half_points - lowest_half_points);

FontSize half_points_size(std::int64_t half_points)
{
    const auto bounded = std::clamp(half_points, lowest_half_points, highest_half_points);
    return FontSize::from_points(static_cast<float>(bounded) /
                                 static_cast<float>(half_points_per_point))
        .value();
}
} // namespace

FontSize adjusted_font_size(FontSize current, FontSizeAdjustment adjustment, std::size_t steps)
{
    const auto amount = static_cast<std::int64_t>(std::min(steps, widest_walk));
    const float doubled = current.points() * static_cast<float>(half_points_per_point);
    // 正の値の切り捨ては床。端数があれば天井は床の 1 つ上。
    const auto floor = static_cast<std::int64_t>(doubled);
    const std::int64_t ceiling = static_cast<float>(floor) < doubled ? floor + 1 : floor;
    switch (adjustment)
    {
    case FontSizeAdjustment::increase:
        return amount == 0 ? current : half_points_size(floor + amount);
    case FontSizeAdjustment::decrease:
        return amount == 0 ? current : half_points_size(ceiling - amount);
    case FontSizeAdjustment::reset:
        return default_font_size();
    }
    std::unreachable();
}

std::string font_size_text(FontSize size)
{
    const auto tenths = static_cast<std::int64_t>((size.points() * 10.0F) + 0.5F);
    std::string text = std::to_string(tenths / 10);
    if (tenths % 10 != 0)
    {
        text += "." + std::to_string(tenths % 10);
    }
    return text;
}

float font_size_dips(FontSize size) noexcept
{
    return size.points() * 96.0F / 72.0F;
}

float font_size_ratio(FontSize size) noexcept
{
    return size.points() / FontSize::default_points;
}
} // namespace nenenib::core
