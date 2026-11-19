#pragma once

#include <cstddef>

namespace nenenib::ui::win32
{
// プレビューの縦の位置（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。block は枠の上端に掛かる
// 要素（錨の要素）、nudge はその要素を置き始める位置（最初の要素は上の余白の上端）から枠の上端
// までの物理画素。正規化した後は 0 以上で、その要素から次の要素の上端までの長さ未満。どちらも
// 単独で妥当なので公開 aggregate（CPP-003）。
struct PreviewScrollSpot
{
    std::size_t block;
    float nudge;
};

[[nodiscard]] inline bool operator==(const PreviewScrollSpot &left,
                                     const PreviewScrollSpot &right) noexcept
{
    return left.block == right.block && left.nudge == right.nudge;
}
} // namespace nenenib::ui::win32
