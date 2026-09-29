#pragma once

#include "DocumentView.hpp"
#include "SaveState.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 帯の位置 from から右へ見て最初の未保存のタブの位置（#237）。無ければ値なし。
// 窓を閉じるとき ui はこれで未保存のタブを左から順に確かめる（ARC-011: 選ぶのは ui の外）。
[[nodiscard]] inline std::optional<std::size_t>
next_unsaved_tab(const std::vector<DocumentView> &tabs, std::size_t from)
{
    for (std::size_t index = from; index < tabs.size(); ++index)
    {
        if (tabs.at(index).save_state != core::SaveState::saved)
        {
            return index;
        }
    }
    return std::nullopt;
}

// 帯の位置 tab のタブが未保存か（範囲の外は false）。タブを閉じるとき ui はこれが true のときだけ
// そのタブへ切り替えて「保存しますか」を出す（ADR 0056 の決定 6）。
[[nodiscard]] inline bool tab_unsaved(const std::vector<DocumentView> &tabs, std::size_t tab)
{
    return tab < tabs.size() && tabs.at(tab).save_state != core::SaveState::saved;
}
} // namespace nenenib::application
