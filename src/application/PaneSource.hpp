#pragma once

#include "AutosaveId.hpp"
#include "EditorState.hpp"
#include "OffsetRange.hpp"
#include "TextViewGeometry.hpp"
#include "VimBlockRange.hpp"
#include "VimPattern.hpp"

#include <optional>

namespace nenenib::application
{
// frame が 1 つの枠の行を作るときに読む値（ADR 0148 の決定 8）。state はその枠を操作中の枠の
// 形で読む状態（もう 1 つの枠なら EditorState::other_pane_state の写し）、range と block は
// 塗る選択、pattern は塗る検索のパターン（無ければ nullptr）。操作中の枠ともう 1 つの枠が同じ
// 関数を通るための束で、frame の中でだけ使う（state と pattern は呼び手が持つ）。tab はその枠の
// 文書の退避番号で、構文の覚えの鍵（ADR 0156 の決定 9）。
struct PaneSource
{
    const EditorState &state;
    core::OffsetRange range;
    std::optional<core::VimBlockRange> block;
    const core::VimPattern *pattern;
    AutosaveId tab;
    // その枠の本文の寸法（操作中の枠は窓から届いた寸法・もう 1 つの枠はその枠の寸法・ADR 0148 の
    // 2026-11-22 の追補の決定 5）。
    const core::TextViewGeometry &geometry;
};
} // namespace nenenib::application
