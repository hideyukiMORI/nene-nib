#include "PaletteRowLayouts.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace nenenib::ui::win32
{
namespace
{
template <typename Value> void append_bytes(std::string &out, Value value)
{
    const auto bytes = std::bit_cast<std::array<char, sizeof(Value)>>(value);
    out.append(bytes.data(), bytes.size());
}

// 鍵の欄を 1
// 本のバイト列に畳む。書式は同一性だけ（指す先の値）、欄は幅と高さだけ（原点は入れない）。
// 先頭の欄は固定の長さなので、文字列を最後に足せば別の鍵が同じバイト列にならない。
void encode(std::string &out, const PaletteRowKey &key)
{
    out.clear();
    append_bytes(out, std::bit_cast<std::uintptr_t>(key.format));
    append_bytes(out, static_cast<std::uint8_t>(key.role));
    append_bytes(out, key.dpi);
    append_bytes(out, core::width_of(key.area));
    append_bytes(out, core::height_of(key.area));
    out.append(key.text);
}
} // namespace

void PaletteRowLayouts::clear()
{
    index_.clear();
    entries_.clear();
}

std::size_t PaletteRowLayouts::size() const noexcept
{
    return entries_.size();
}

PaletteRowLayouts::TextLayout PaletteRowLayouts::lookup(const PaletteRowKey &key)
{
    encode(probe_, key);
    const auto found = index_.find(std::string_view{probe_});
    if (found == index_.end())
    {
        return nullptr;
    }
    entries_.splice(entries_.begin(), entries_, found->second);
    return found->second->layout;
}

void PaletteRowLayouts::retain(const PaletteRowKey &key, const TextLayout &layout)
{
    if (layout == nullptr)
    {
        return;
    }
    if (!entries_.empty() && entries_.size() >= limit_)
    {
        index_.erase(std::string_view{entries_.back().key});
        entries_.pop_back();
    }
    std::string encoded;
    encode(encoded, key);
    entries_.push_front(PaletteRowLayout{std::move(encoded), layout});
    index_.insert_or_assign(std::string_view{entries_.front().key}, entries_.begin());
}
} // namespace nenenib::ui::win32
