#pragma once

#include "Offset.hpp"
#include "VimAction.hpp"
#include "VimEditorView.hpp"
#include "VimLayoutNeed.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <expected>
#include <optional>

namespace nenenib::core
{
// 窓と wrap off の横送り `zl` `zh` `zL` `zH` `zs` `ze` の結果（ADR 0145 の決定 6）。
// left_column は新しい leftcol（升・0 始まり）、caret は画面の外に出たら端の文字へ押した
// キャレット、pushed は押す規則が当たったか（当たったときだけ欲しい列を置き直す）。
struct VimHorizontalScroll
{
    std::size_t left_column;
    Offset caret;
    bool pushed = false;
};

// 横送りの鍵を Vim 9.1 の実測（probe-634・sidescrolloff=0）のとおりに解く。横送りでない
// 動作と横に送らない文書（VimViewport.columns == 0）は nullopt（何もしない・失敗にしない）。
// 新しい leftcol は [0, キャレットの行の最後の文字の最初の升] に収める。zl zh zL zH は
// キャレットの文字が [leftcol, leftcol + columns) に収まらなければ端の文字へ押す（左は
// leftcol で始まる文字・右は leftcol + columns − 1 で終わる文字・ADR 0053 の 1 文字）。
// zs ze は押さない。leftcol が変わらなければキャレットも欲しい列も変えない。キャレットの
// 画面行の答え（VimRowSpan の width と column）が無ければその問いを返す（文字組みの有効か
// 否かと独立・決定 4）。行を写さない（offset_at_virtual_column と 1 文字の読みだけ）。
[[nodiscard]] std::expected<std::optional<VimHorizontalScroll>, VimLayoutNeed>
vim_horizontal_scroll(const VimState &state, const VimEditorView &view, VimAction action);
} // namespace nenenib::core
