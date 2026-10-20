#pragma once

#include "EditorState.hpp"
#include "OffsetRange.hpp"
#include "VimBlockRange.hpp"
#include "VimPattern.hpp"

#include <optional>

namespace nenenib::application
{
// frame が 1 つの枠の行を作るときに読む値（ADR 0148 の決定 8）。state はその枠を操作中の枠の
// 形で読む状態（もう 1 つの枠なら EditorState::other_pane_state の写し）、range と block は
// 塗る選択、pattern は塗る検索のパターン（無ければ nullptr）。操作中の枠ともう 1 つの枠が同じ
// 関数を通るための束で、frame の中でだけ使う（state と pattern は呼び手が持つ）。
struct PaneSource
{
    const EditorState &state;
    core::OffsetRange range;
    std::optional<core::VimBlockRange> block;
    const core::VimPattern *pattern;
};
} // namespace nenenib::application
