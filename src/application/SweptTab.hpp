#pragma once

#include "AutosaveId.hpp"
#include "TabSweep.hpp"
#include "TabSweepSide.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::application
{
// 帯（退避番号の並び）の中の tab の位置。無ければ値なし。
[[nodiscard]] inline std::optional<std::size_t> tab_position(const std::vector<AutosaveId> &band,
                                                             AutosaveId tab)
{
    const auto found = std::ranges::find(band, tab);
    if (found == band.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(band.begin(), found));
}

// 帯の位置 position のタブが sweep で閉じる相手か（ADR 0078 の追補 #1790）。残すタブが帯に無ければ
// どれも閉じない。
[[nodiscard]] inline bool tab_swept(const std::vector<AutosaveId> &band, const TabSweep &sweep,
                                    std::size_t position)
{
    const auto keep = tab_position(band, sweep.keep);
    if (!keep.has_value() || position >= band.size() || position == keep.value())
    {
        return false;
    }
    switch (sweep.side)
    {
    case TabSweepSide::others:
        return true;
    case TabSweepSide::right:
        return position > keep.value();
    }
    std::unreachable();
}

// sweep で次に閉じるタブの帯の位置（帯の左から順に最初の相手）。無ければ値なし。ui は 1 つ閉じる
// たびにこれを引き直す（閉じると位置がずれる・ARC-011: 選ぶのは ui の外）。
[[nodiscard]] inline std::optional<std::size_t> swept_tab(const std::vector<AutosaveId> &band,
                                                          const TabSweep &sweep)
{
    for (std::size_t position = 0; position < band.size(); ++position)
    {
        if (tab_swept(band, sweep, position))
        {
            return position;
        }
    }
    return std::nullopt;
}
} // namespace nenenib::application
