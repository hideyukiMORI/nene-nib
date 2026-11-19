#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// マウスを載せた表の横の送りの棒（#1883）。table は表の通し番号で、棒の外は値なし。変わったとき
// だけ ui が送る（面の行の HoverExplorer と同じ形）。使う人の操作ではない（not_user_operation）。
struct HoverPreviewTable
{
    std::optional<std::size_t> table;
};
} // namespace nenenib::application
