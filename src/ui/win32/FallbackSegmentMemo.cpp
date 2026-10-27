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

// 字体の family の名前（en-us、無ければ最初の名前）。読めなければ空。
[[nodiscard]] std::wstring english_family(IDWriteFont &font)
{
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
    UINT32 index = 0;
    BOOL exists = FALSE;
    UINT32 length = 0;
    if (FAILED(font.GetFontFamily(&family)) || FAILED(family->GetFamilyNames(&names)) ||
        FAILED(names->FindLocaleName(L"en-us", &index, &exists)))
    {
        return {};
    }
    index = exists != FALSE ? index : 0;
    if (FAILED(names->GetStringLength(index, &length)))
    {
        return {};
    }
    std::wstring name(length + 1, L'\0');
    if (FAILED(names->GetString(index, name.data(), length + 1)))
    {
        return {};
    }
    name.resize(length);
    return name;
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

std::optional<std::size_t> FallbackSegmentMemo::font(std::size_t entry) const
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

void FallbackSegmentMemo::remember(std::wstring_view text, std::optional<std::size_t> font,
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
    fonts_.push_back(font);
    scales_.push_back(scale);
    mapped_.push_back(mapped);
}

std::optional<std::size_t>
FallbackSegmentMemo::normalized(const Microsoft::WRL::ComPtr<IDWriteFont> &font)
{
    if (font == nullptr)
    {
        return std::nullopt;
    }
    const auto same = std::ranges::find(row_fonts_, font);
    if (same != row_fonts_.end())
    {
        return static_cast<std::size_t>(same - row_fonts_.begin());
    }
    auto family = english_family(*font.Get());
    // 読み取りの一時失敗も null として覚えに残る（次の coverage_ の作り直しで消える・#889）。
    if (family.empty())
    {
        return std::nullopt;
    }
    const auto weight = font->GetWeight();
    const auto style = font->GetStyle();
    const auto stretch = font->GetStretch();
    const auto found = row_of(family, weight, style, stretch);
    if (found.has_value())
    {
        return found;
    }
    row_fonts_.push_back(font);
    row_families_.push_back(std::move(family));
    row_weights_.push_back(weight);
    row_styles_.push_back(style);
    row_stretches_.push_back(stretch);
    return row_fonts_.size() - 1;
}

std::optional<std::size_t> FallbackSegmentMemo::row_of(const std::wstring &family,
                                                       DWRITE_FONT_WEIGHT weight,
                                                       DWRITE_FONT_STYLE style,
                                                       DWRITE_FONT_STRETCH stretch) const
{
    for (std::size_t row = 0; row < row_fonts_.size(); ++row)
    {
        if (row_weights_.at(row) == weight && row_styles_.at(row) == style &&
            row_stretches_.at(row) == stretch && row_families_.at(row) == family)
        {
            return row;
        }
    }
    return std::nullopt;
}

const Microsoft::WRL::ComPtr<IDWriteFont> &FallbackSegmentMemo::row_font(std::size_t row) const
{
    return row_fonts_.at(row);
}

const std::wstring &FallbackSegmentMemo::row_family(std::size_t row) const
{
    return row_families_.at(row);
}

DWRITE_FONT_WEIGHT FallbackSegmentMemo::row_weight(std::size_t row) const
{
    return row_weights_.at(row);
}

DWRITE_FONT_STYLE FallbackSegmentMemo::row_style(std::size_t row) const
{
    return row_styles_.at(row);
}

DWRITE_FONT_STRETCH FallbackSegmentMemo::row_stretch(std::size_t row) const
{
    return row_stretches_.at(row);
}

std::size_t FallbackSegmentMemo::rows() const noexcept
{
    return row_fonts_.size();
}

// 上限に達したら全部捨てる（単純で決定的・ADR 0142 の追補）。正規の字体の表は残す（行の番号を
// 持つのは覚えの entry と PrimaryFontCoverage の当てる前の範囲だけで、表は縮まない）。
void FallbackSegmentMemo::forget()
{
    std::ranges::fill(slots_, 0U);
    texts_.clear();
    fonts_.clear();
    scales_.clear();
    mapped_.clear();
}
} // namespace nenenib::ui::win32
