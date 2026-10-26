#include "FallbackPiece.hpp"

#include "DisplayWidth.hpp"
#include "FontFallbackKey.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstdint>

namespace nenenib::ui::win32
{
namespace
{
constexpr wchar_t high_surrogate_first = 0xD800;
constexpr wchar_t high_surrogate_last = 0xDBFF;
constexpr wchar_t low_surrogate_first = 0xDC00;
constexpr wchar_t low_surrogate_last = 0xDFFF;
constexpr char32_t surrogate_base = 0x10000;
constexpr unsigned surrogate_shift = 10;
// 切り目の hash（乗算ハッシュの上位 6 ビット・当たりは 0 で平均 64 単位に 1 つ）。
constexpr std::uint32_t piece_hash_multiplier = 0x9E3779B1U;
constexpr unsigned piece_hash_shift = 26;
// hash の取り得る値より大きい印（まだ切ってよい所を見ていない）。
constexpr std::uint32_t no_piece_hash = 1U << (32U - piece_hash_shift);

[[nodiscard]] constexpr std::uint32_t piece_hash(char32_t value)
{
    return (static_cast<std::uint32_t>(value) * piece_hash_multiplier) >> piece_hash_shift;
}

// 上限で位置で切る（切り目が前の文字に続くなら、その文字の前まで戻す）。
[[nodiscard]] UINT32 limit_length(std::wstring_view wide, UINT32 start, UINT32 remaining)
{
    auto length = std::min(remaining, fallback_key_text_limit);
    while (length > 1 && length < remaining && wide_continues_at(wide, start + length))
    {
        --length;
    }
    return length;
}

// start から length 単位の後ろで切ってよいか（最小以上・上限未満・次の単位が前の文字に続かない）。
[[nodiscard]] bool may_cut(std::wstring_view wide, UINT32 start, UINT32 length, UINT32 limit)
{
    return length >= fallback_piece_minimum && length < limit &&
           !wide_continues_at(wide, start + length);
}
} // namespace

std::pair<char32_t, UINT32> wide_code_point_at(std::wstring_view wide, std::size_t unit)
{
    const auto first = wide[unit];
    if (first < high_surrogate_first || first > high_surrogate_last || unit + 1 >= wide.size() ||
        wide[unit + 1] < low_surrogate_first || wide[unit + 1] > low_surrogate_last)
    {
        return {static_cast<char32_t>(first), 1U};
    }
    const auto high = static_cast<char32_t>(first - high_surrogate_first);
    const auto low = static_cast<char32_t>(wide[unit + 1] - low_surrogate_first);
    return {surrogate_base + (high << surrogate_shift) + low, 2U};
}

bool wide_continues_at(std::wstring_view wide, std::size_t unit)
{
    const auto value = wide[unit];
    return (value >= low_surrogate_first && value <= low_surrogate_last) ||
           core::display_width(wide_code_point_at(wide, unit).first) == core::DisplayWidth::zero;
}

UINT32 fallback_piece_length(std::wstring_view wide, UINT32 start, UINT32 remaining)
{
    if (remaining <= fallback_piece_minimum)
    {
        return remaining;
    }
    const auto limit = std::min(remaining, fallback_key_text_limit);
    std::pair<std::uint32_t, UINT32> lowest{no_piece_hash, 0};
    for (UINT32 length = 0; length < limit;)
    {
        const auto [value, units] = wide_code_point_at(wide, start + length);
        length += units;
        if (!may_cut(wide, start, length, limit))
        {
            continue;
        }
        const auto hash = piece_hash(value);
        if (hash == 0)
        {
            return length;
        }
        lowest = hash <= lowest.first ? std::pair{hash, length} : lowest;
    }
    if (limit == remaining)
    {
        return remaining;
    }
    return lowest.second != 0 ? lowest.second : limit_length(wide, start, remaining);
}
} // namespace nenenib::ui::win32
