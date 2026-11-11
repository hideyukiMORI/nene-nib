#pragma once

#include "StartEntry.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace nenenib::core
{
// 起動画面の行の上の押し（ADR 0172 の追補 2026-11-11 の 24・PR #1459 のレビュー M3）。押した行の
// 位置と、その行の中身（種類と path）を覚える。押してから離すまでに行の列が変わっても（履歴の読みが
// 届く・読みの失敗で行が外れる）、押していない行を実行しないために中身を比べる。公開 aggregate
// （CPP-003）。
struct StartPress
{
    std::size_t index;
    StartEntry entry;
};

// 押した点の行 row（start_screen_row_at の答え）から押しを作る。行の外（値なし）か範囲の外なら
// 値なし。
[[nodiscard]] std::optional<StartPress> start_pressed(std::span<const StartEntry> entries,
                                                      std::optional<std::size_t> row);

// 離した点の行 row が押した行と同じ位置で、今の列のその行の中身が押した行と同じときだけ、
// 実行する行の位置を返す（ADR 0056 の決定 9 の「押した要素と離した要素が同じとき」）。
// 押していない・行の外・中身が違う（列が変わった）なら値なし。
[[nodiscard]] std::optional<std::size_t> start_released(const std::optional<StartPress> &pressed,
                                                        std::span<const StartEntry> entries,
                                                        std::optional<std::size_t> row) noexcept;
} // namespace nenenib::core
