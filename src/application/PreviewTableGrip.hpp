#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// つかんでいる表の横の送りのつまみ（#1883）。table は表の通し番号、pointer はつかんだ点の溝の
// 左端からの位置（DIP）、offset はつかんだときの送りの量（DIP）。窓の値で、離す・つまみの外の押し・
// 結果の差し替えで消える。公開 aggregate（CPP-003）。
struct PreviewTableGrip
{
    std::size_t table;
    std::int32_t pointer;
    std::int32_t offset;
};

[[nodiscard]] inline bool operator==(const PreviewTableGrip &left,
                                     const PreviewTableGrip &right) noexcept
{
    return left.table == right.table && left.pointer == right.pointer &&
           left.offset == right.offset;
}
} // namespace nenenib::application
