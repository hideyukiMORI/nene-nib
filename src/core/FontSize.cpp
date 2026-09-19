#include "FontSize.hpp"

#include <algorithm>
#include <charconv>
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

FontSize adjusted_font_size(FontSize current, FontSizeAdjustment adjustment, std::size_t steps)
{
    const float amount = static_cast<float>(std::min<std::size_t>(steps, 40));
    switch (adjustment)
    {
    case FontSizeAdjustment::increase:
        return FontSize::from_points(std::min(current.points() + amount, FontSize::maximum_points))
            .value();
    case FontSizeAdjustment::decrease:
        return FontSize::from_points(std::max(current.points() - amount, FontSize::minimum_points))
            .value();
    case FontSizeAdjustment::reset:
        return default_font_size();
    }
    std::unreachable();
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
