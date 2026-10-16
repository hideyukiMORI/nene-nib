#include "FontFallbackTable.hpp"
#include <utility>
namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t maximum_entries = 128;
}

FontFallbackTable::FontFallbackTable()
{
    entries_.reserve(maximum_entries);
    slots_.reserve(maximum_entries);
}

const FontFallbackEntry *FontFallbackTable::find(const FontFallbackKey &key) const
{
    const auto [first, last] = slots_.equal_range(key.hash);
    for (auto slot = first; slot != last; ++slot)
    {
        const auto &entry = entries_.at(slot->second);
        if (entry.key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

void FontFallbackTable::retain(FontFallbackEntry entry)
{
    const std::size_t hash = entry.key.hash;
    if (entries_.size() < maximum_entries)
    {
        entries_.push_back(std::move(entry));
        slots_.emplace(hash, entries_.size() - 1);
        return;
    }
    const auto [first, last] = slots_.equal_range(entries_.at(next_).key.hash);
    for (auto slot = first; slot != last; ++slot)
    {
        if (slot->second == next_)
        {
            slots_.erase(slot);
            break;
        }
    }
    entries_.at(next_) = std::move(entry);
    slots_.emplace(hash, next_);
    next_ = (next_ + 1) % maximum_entries;
}
} // namespace nenenib::ui::win32
