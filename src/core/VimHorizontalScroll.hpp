#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"
#include "VimAction.hpp"
#include "VimEditorView.hpp"
#include "VimLayoutNeed.hpp"
#include "VimState.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <utility>

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

// 行 line の 0 始まりの升 column を含む文字の先頭（行より右なら行の内容の終わり）。横送りの押しと
// `g0` `gm` が共用する（ADR 0145 の決定 6・ADR 0147 の決定 2）。左端で割れた全角はその字。
[[nodiscard]] Offset character_at_cell(const TextBuffer &text, LineNumber line, std::size_t column);

// 行の内容の範囲 line = [start, end) の中で、右端の升 right を含む文字。そこで終わらなければ前の
// 文字（ADR 0053 の 1 文字）で、NORMAL の寄せ（最後の文字）を済ませた位置。横送りの押しと `g$`
// が共用する（ADR 0145 の決定 6・ADR 0147 の決定 2）。
[[nodiscard]] Offset right_edge_character(const TextBuffer &text, std::pair<Offset, Offset> line,
                                          std::size_t right);
} // namespace nenenib::core
