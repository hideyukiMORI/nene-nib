#pragma once

#include <algorithm>
#include <cstddef>

namespace nenenib::core
{
// ミニマップの窓の中で描き直す帯（ADR 0171 の決定 4）。窓の中の行の番号（0 始まり）で、前の行
// [begin, begin + removed) が新しい行 [begin, begin + inserted) に替わった。その下の行は
// inserted - removed だけずれる（中身はそのまま）。どの欄も単独で妥当な値なので公開 aggregate。
struct MinimapBand
{
    std::size_t begin;
    std::size_t removed;
    std::size_t inserted;
};

[[nodiscard]] constexpr bool operator==(const MinimapBand &left, const MinimapBand &right) noexcept
{
    return left.begin == right.begin && left.removed == right.removed &&
           left.inserted == right.inserted;
}

// 帯 first の後に帯 then（first の後の行の番号）が来たときの、両方を合わせた 1 つの帯。前の行から
// 見て、合わせた帯の外の行は替わっていない（ずれるだけ）。
[[nodiscard]] constexpr MinimapBand minimap_band_joined(const MinimapBand &first,
                                                        const MinimapBand &then) noexcept
{
    const std::size_t begin = std::min(first.begin, then.begin);
    const std::size_t end = std::max(first.begin + first.inserted, then.begin + then.removed);
    // end は first の後の行の番号で、first の帯より下なので前の行へは first のずれを戻す。
    const std::size_t old_end = end + first.removed - first.inserted;
    const std::size_t new_end = end + then.inserted - then.removed;
    return MinimapBand{begin, old_end - begin, new_end - begin};
}
} // namespace nenenib::core
