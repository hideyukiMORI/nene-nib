#pragma once

#include "AutosaveId.hpp"
#include "TabSweep.hpp"
#include "TabSweepMove.hpp"
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

// まとめて閉じる道の次の一歩と、その帯の位置（ADR 0078 の追補 #1790 の決定 6）。アクティブでない
// 相手を帯の左から先に閉じ（位置はその相手）、相手がアクティブだけになったら残すタブへ切り替える
// （位置は残すタブ）。相手が無ければ done（位置は 0）。ui は 1 歩ごとにこれを引き直し、切り替えの
// 後にアクティブが残すタブでなければ（裏で読んでいる・読めずに外れた）そこで止める。
[[nodiscard]] inline std::pair<TabSweepMove, std::size_t>
tab_sweep_step(const std::vector<AutosaveId> &band, const TabSweep &sweep, std::size_t active)
{
    bool active_swept = false;
    for (std::size_t position = 0; position < band.size(); ++position)
    {
        if (!tab_swept(band, sweep, position))
        {
            continue;
        }
        if (position != active)
        {
            return {TabSweepMove::close, position};
        }
        active_swept = true;
    }
    if (!active_swept)
    {
        return {TabSweepMove::done, 0};
    }
    return {TabSweepMove::switch_to_keep, tab_position(band, sweep.keep).value_or(0)};
}
} // namespace nenenib::application
