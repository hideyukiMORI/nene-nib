#include "PaletteRowLayouts.hpp"

#include <algorithm>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] core::LayoutRect dimensions(const core::LayoutRect &area)
{
    return {0, 0, core::width_of(area), core::height_of(area)};
}

[[nodiscard]] bool same_key(const PaletteRowLayout &entry, const PaletteRowKey &key)
{
    return entry.format == key.format && entry.role == key.role && entry.dpi == key.dpi &&
           entry.area == dimensions(key.area) && entry.text == key.text;
}
} // namespace

void PaletteRowLayouts::clear()
{
    entries_.clear();
}

std::size_t PaletteRowLayouts::size() const noexcept
{
    return entries_.size();
}

PaletteRowLayouts::TextLayout PaletteRowLayouts::lookup(const PaletteRowKey &key)
{
    const auto found = std::ranges::find_if(entries_, [&](const PaletteRowLayout &entry)
                                            { return same_key(entry, key); });
    if (found == entries_.end())
    {
        return nullptr;
    }
    ++generation_;
    found->used = generation_;
    return found->layout;
}

void PaletteRowLayouts::retain(const PaletteRowKey &key, const TextLayout &layout)
{
    if (layout == nullptr)
    {
        return;
    }
    if (entries_.size() >= capacity)
    {
        entries_.erase(std::ranges::min_element(entries_, {}, &PaletteRowLayout::used));
    }
    ++generation_;
    entries_.push_back({key.format, key.role, std::string{key.text}, dimensions(key.area), key.dpi,
                        generation_, layout});
}
} // namespace nenenib::ui::win32
