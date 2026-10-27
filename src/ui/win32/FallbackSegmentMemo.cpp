#include "FallbackSegmentMemo.hpp"

#include "FontFallbackKey.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::size_t slot_count = fallback_segment_memo_limit * 2;
constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr unsigned byte_bits = 8;
constexpr std::uint64_t byte_mask = 0xFF;

// UTF-16 の単位ごとに下位・上位の 2 バイトを FNV-1a で混ぜる。
[[nodiscard]] std::uint64_t fnv1a(std::wstring_view text) noexcept
{
    auto hash = fnv_offset;
    for (const auto unit : text)
    {
        const auto value = static_cast<std::uint64_t>(unit);
        hash = (hash ^ (value & byte_mask)) * fnv_prime;
        hash = (hash ^ ((value >> byte_bits) & byte_mask)) * fnv_prime;
    }
    return hash;
}
} // namespace

FallbackSegmentMemo::FallbackSegmentMemo() : slots_(slot_count, 0U)
{
    texts_.reserve(fallback_segment_memo_limit);
    fonts_.reserve(fallback_segment_memo_limit);
    scales_.reserve(fallback_segment_memo_limit);
    mapped_.reserve(fallback_segment_memo_limit);
}

// 文字列の番地（覚えていればその番地、覚えていなければ最初の空き）。番地の半分以上は常に空く。
// 片は本文の任意の単位から始まるので、比べるのは単位ごと（UCRT の wmemcmp は 8 バイトに揃って
// いない番地を 8 バイトで読み、UBSan が落とす）。
std::size_t FallbackSegmentMemo::slot_of(std::wstring_view text) const
{
    auto slot = static_cast<std::size_t>(fnv1a(text) & (slot_count - 1));
    while (slots_.at(slot) != 0U && !std::ranges::equal(texts_.at(slots_.at(slot) - 1U), text))
    {
        slot = (slot + 1) & (slot_count - 1);
    }
    return slot;
}

std::optional<std::size_t> FallbackSegmentMemo::find(std::wstring_view text) const
{
    const auto entry = slots_.at(slot_of(text));
    if (entry == 0U)
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(entry - 1U);
}

const Microsoft::WRL::ComPtr<IDWriteFont> &FallbackSegmentMemo::font(std::size_t entry) const
{
    return fonts_.at(entry);
}

FLOAT FallbackSegmentMemo::scale(std::size_t entry) const
{
    return scales_.at(entry);
}

UINT32 FallbackSegmentMemo::mapped(std::size_t entry) const
{
    return mapped_.at(entry);
}

std::size_t FallbackSegmentMemo::size() const noexcept
{
    return texts_.size();
}

void FallbackSegmentMemo::remember(std::wstring_view text, Microsoft::WRL::ComPtr<IDWriteFont> font,
                                   FLOAT scale, UINT32 mapped)
{
    if (text.empty() || text.size() > fallback_key_text_limit || find(text).has_value())
    {
        return;
    }
    if (texts_.size() >= fallback_segment_memo_limit)
    {
        forget();
    }
    slots_.at(slot_of(text)) = static_cast<std::uint32_t>(texts_.size() + 1);
    texts_.emplace_back(text);
    fonts_.push_back(std::move(font));
    scales_.push_back(scale);
    mapped_.push_back(mapped);
}

// 上限に達したら全部捨てる（単純で決定的・ADR 0142 の追補）。
void FallbackSegmentMemo::forget()
{
    std::ranges::fill(slots_, 0U);
    texts_.clear();
    fonts_.clear();
    scales_.clear();
    mapped_.clear();
}
} // namespace nenenib::ui::win32
