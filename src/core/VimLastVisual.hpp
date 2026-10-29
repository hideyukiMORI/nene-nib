#pragma once

#include "VimMarkPosition.hpp"
#include "VimMode.hpp"
#include "VimWantedColumn.hpp"

#include <optional>

namespace nenenib::core
{
// 前の選択（ADR 0018 の追補・#1039）。`gv` が選び直す VISUAL の種類と両端と欲しい列で、Vim の
// b_visual（vi_mode・vi_start・vi_end・vi_curswant）と同じく文書ごとに 1 つ持つ（VimMarks の欄）。
// anchor は選択を始めた端、caret はキャレットの端。矩形の `$` は wanted の at_line_end で残る。
// mode は visual・visual_line・visual_block のどれか。どの値も単独で妥当なので公開 aggregate
// （CPP-003）。比較は非メンバー。
struct VimLastVisual
{
    VimMode mode;
    VimMarkPosition anchor;
    VimMarkPosition caret;
    std::optional<VimWantedColumn> wanted;
};

[[nodiscard]] constexpr bool operator==(const VimLastVisual &left,
                                        const VimLastVisual &right) noexcept
{
    return left.mode == right.mode && left.anchor == right.anchor && left.caret == right.caret &&
           left.wanted == right.wanted;
}
} // namespace nenenib::core
