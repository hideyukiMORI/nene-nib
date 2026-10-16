#pragma once

#include "OffsetRange.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 1 行ぶんの塗る範囲を本文の位置で持つ（ADR 0140 の決定 5）。選択（矩形のあいだは行ごとの範囲）・
// 検索の当たり（hlsearch）・キャレットを含む当たり（incsearch の preview を含む）。
struct GridLineMarks
{
    core::OffsetRange selection;
    std::vector<core::OffsetRange> matches;
    std::optional<core::OffsetRange> current;
    // 当たりを照合するのに本文から読んだバイト数（EditorFrame::grid_searched の 1 行ぶん）。
    std::size_t searched;
};
} // namespace nenenib::application
