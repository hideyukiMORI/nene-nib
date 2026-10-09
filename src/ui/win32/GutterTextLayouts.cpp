#include "GutterTextLayouts.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] core::LayoutRect dimensions(const core::LayoutRect &area)
{
    return {0, 0, core::width_of(area), core::height_of(area)};
}
} // namespace

void GutterTextLayouts::begin()
{
    previous_.swap(current_);
    current_.clear();
}

void GutterTextLayouts::end()
{
    previous_.clear();
}

void GutterTextLayouts::clear()
{
    current_.clear();
    previous_.clear();
}

std::optional<Microsoft::WRL::ComPtr<IDWriteTextLayout>>
GutterTextLayouts::lookup(std::string_view text, const core::LayoutRect &area)
{
    const auto key = dimensions(area);
    const auto matches = [&](const GutterTextLayout &entry)
    { return entry.layout != nullptr && entry.text == text && entry.area == key; };
    const auto current = std::ranges::find_if(current_, matches);
    if (current != current_.end())
    {
        return current->layout;
    }
    const auto previous = std::ranges::find_if(previous_, matches);
    if (previous == previous_.end())
    {
        return std::nullopt;
    }
    current_.push_back(std::move(*previous));
    return current_.back().layout;
}

void GutterTextLayouts::retain(std::string_view text, const core::LayoutRect &area,
                               Microsoft::WRL::ComPtr<IDWriteTextLayout> layout)
{
    if (layout != nullptr && !lookup(text, area).has_value())
    {
        current_.push_back({std::string{text}, dimensions(area), std::move(layout)});
    }
}
} // namespace nenenib::ui::win32
