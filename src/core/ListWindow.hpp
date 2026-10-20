#pragma once

#include <algorithm>
#include <cstddef>

namespace nenenib::core
{
// 一覧の見えている窓の先頭（ADR 0150 の決定 1）。Ctrl+P の面と `:` の補完の面が共有する 1 つの規則
// （ARC-001）。選択 selected が窓 [first, first + visible) の中ならそのまま、上に出るなら
// first = selected、下に出るなら first = selected − visible + 1。窓は件数 total の末尾を越えて
// 空の行を残さない（窓が広がったときも末尾の候補が最下段）。visible が 0 なら 1 行とみなす。
[[nodiscard]] constexpr std::size_t list_window_first(std::size_t first, std::size_t selected,
                                                      std::size_t visible,
                                                      std::size_t total) noexcept
{
    const std::size_t rows = std::max(visible, std::size_t{1});
    const std::size_t last_first = total > rows ? total - rows : 0;
    const std::size_t kept = std::min(first, last_first);
    if (selected >= total)
    {
        return kept;
    }
    if (selected < kept)
    {
        return selected;
    }
    if (selected >= kept + rows)
    {
        return selected + 1 - rows;
    }
    return kept;
}
} // namespace nenenib::core
