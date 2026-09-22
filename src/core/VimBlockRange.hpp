#pragma once

#include "LineNumber.hpp"
#include "Selection.hpp"
#include "TextBuffer.hpp"
#include "VimBlockLine.hpp"
#include "VimBlockWidth.hpp"
#include "VimColumnWish.hpp"
#include "VimRemoveBlock.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"

#include <optional>
#include <string>
#include <vector>

namespace nenenib::core
{
// 矩形 VISUAL が覆う本文（ADR 0035 の決定 2）。描く選択も Ctrl+C / Ctrl+X も `d x y r` も
// `.` の大きさも、この 1 本が決めた行ごとの範囲を使う（ARC-001）。純関数。
//
// `lines` は上の行から 1 行ずつで、`first` がその最初の行。`left` は矩形の左の仮想桁、
// `width` は覆う桁の数（1 以上）。
struct VimBlockRange
{
    std::vector<VimBlockLine> lines;
    LineNumber first;
    VirtualColumn left;
    VimBlockWidth width;
};

// 選択の 2 つの角を仮想桁の矩形と読む。左右は角の文字の「最初の桁」と「最後の桁」の小さいほうと
// 大きいほうで、`wish` が at_line_end（`$`）なら右端は覆う行のうちいちばん長い行の終わりになる
// （固定 Vim も取るときに幅へ畳む・Issue #112 で実測）。
[[nodiscard]] VimBlockRange vim_block_range(const TextBuffer &text, const Selection &selection,
                                            VimColumnWish wish);

// VimState が覚えている欲しい列から矩形を決める 1 本。engine の `d x y r` と、描画・
// Ctrl+C / Ctrl+X がここを通る（ARC-001）。
[[nodiscard]] VimBlockRange vim_block_range_for(const TextBuffer &text, const Selection &selection,
                                                const std::optional<VimWantedColumn> &wanted);

// 矩形が覆った本文（レジスタに入る形）。行を LF でつなぎ、末尾に改行は付けない。
[[nodiscard]] std::string vim_block_text(const TextBuffer &text, const VimBlockRange &block);

// 矩形を消す効果（ADR 0035 の決定 4）。端で切れた文字は丸ごと消えるので、矩形の外側だった桁を
// 空白で埋め直す（Issue #112 で実測）。engine の `d` `x` と窓の Ctrl+X が同じ 1 本を通る。
[[nodiscard]] VimRemoveBlock vim_remove_block(const VimBlockRange &block);
} // namespace nenenib::core
