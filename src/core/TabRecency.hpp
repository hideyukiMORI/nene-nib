#pragma once

#include "TabStep.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nenenib::core
{
// タブの使った順（ADR 0058 の決定 1）。帯の位置（0 始まり）を「いちばん最近」から順に並べた列で、
// 開いている全部のタブを 1 回ずつ含む（不変条件）。作る口は 1 本のときの single と下の
// 純関数だけで、どれも不変条件を保った列しか作らない（CPP-007）。範囲の外の位置は状態を
// 変えない。
class TabRecency final
{
  public:
    // タブが 1 本（帯の位置 0）だけのときの列。
    [[nodiscard]] static TabRecency single();
    // 帯の位置を使った順に並べた読み取りの列。先頭がいちばん最近。
    [[nodiscard]] std::span<const std::size_t> order() const & noexcept;

    friend TabRecency tab_recency_touched(const TabRecency &recency, std::size_t tab);
    friend TabRecency tab_recency_opened(const TabRecency &recency, std::size_t tab);
    friend TabRecency tab_recency_closed(const TabRecency &recency, std::size_t tab);

  private:
    explicit TabRecency(std::vector<std::size_t> order);
    std::vector<std::size_t> order_;
};

// 帯の位置 tab のタブを先頭へ動かす。範囲の外なら同じ列。
[[nodiscard]] TabRecency tab_recency_touched(const TabRecency &recency, std::size_t tab);
// 帯の位置 tab に新しいタブができた。それ以上の位置は 1 つずつ後ろへずれ、新しいタブは先頭。
// tab は 0 から本数まで（末尾の右も足せる）。範囲の外なら同じ列。
[[nodiscard]] TabRecency tab_recency_opened(const TabRecency &recency, std::size_t tab);
// 帯の位置 tab のタブが閉じた。それより後ろの位置は 1 つずつ前へずれる。範囲の外と最後の
// 1 本なら同じ列（最後の 1 本は閉じない・ADR 0056 の決定 6）。
[[nodiscard]] TabRecency tab_recency_closed(const TabRecency &recency, std::size_t tab);
// 列の上で from の隣の帯の位置。TabStep::next は「より前に使った」ほう、previous は逆。端は
// 折り返し、1 本のときは同じ位置。from が列に無ければ値なし（tab_destination と同じ流儀）。
[[nodiscard]] std::optional<std::size_t> tab_recency_walked(const TabRecency &recency,
                                                            std::size_t from, TabStep step);
} // namespace nenenib::core
