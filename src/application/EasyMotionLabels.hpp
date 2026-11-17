#pragma once

#include "EditorState.hpp"
#include "LineView.hpp"

#include <vector>

namespace nenenib::application
{
// イージーモーションの印を見えている行の LineView.labels に写す（ADR 0163 の決定 6）。印が出て
// いる（VimState.easy_motion が在る）間だけで、まだ打った接頭辞で始まる印だけを、残りの文字で
// 載せる。桁は colors と同じ code point の序数（巨大な行の窓では窓の中の桁・窓の外の跳び先は
// 載せない）、cells は跳び先の文字の升の数（全角は 2・行の終わりと空の行は 1）。lines
// は操作中の枠の text の表示の行。
void mark_labels(const EditorState &state, std::vector<LineView> &lines);
} // namespace nenenib::application
