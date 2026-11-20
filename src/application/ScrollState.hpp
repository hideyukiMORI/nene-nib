#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::application
{
// 縦スクロールの状態。application が所有し、上限は core の純関数が決める（ADR 0009 の決定 6）。
// 先頭行と見えている行数は互いに独立して妥当なので公開 aggregate（ADR 0007）。
struct ScrollState
{
    core::LineNumber first_visible;
    std::size_t visible_lines;
    std::size_t first_row = 0;
    float horizontal = 0;
    // 先頭の画面行が上へはみ出している量（1/core::scroll_fraction_units 行・0 以上その未満・
    // #1898）。ホイールとタッチパッドの送り（ScrollLines / ScrollPane の parts）だけが 0 以外を
    // 置き、行の単位で窓を決める道（鍵・追従・収め・タブの復元）は 0 に戻す。本文の端（先頭と送りの
    // 上限）では 0。Vim の engine（VimViewport）と oracle には見せない。
    std::size_t fraction = 0;
};

[[nodiscard]] constexpr bool operator==(const ScrollState &left, const ScrollState &right) noexcept
{
    return left.first_visible == right.first_visible && left.visible_lines == right.visible_lines &&
           left.first_row == right.first_row && left.horizontal == right.horizontal &&
           left.fraction == right.fraction;
}
} // namespace nenenib::application
