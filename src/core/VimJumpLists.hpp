#pragma once

#include "VimJumpList.hpp"
#include "VimJumpMove.hpp"
#include "VimMarkPosition.hpp"
#include "VimMarks.hpp"

#include <cstddef>

namespace nenenib::core
{
// ジャンプの列と変更の列を読み書きする純関数（ADR 0152 の追補 #1037）。列の意味を決めるのはこの
// 1 か所だけ（ARC-001）。engine は `Ctrl-o` `Ctrl-i` `g;` `g,` と飛ぶ鍵（vim_marks_jumped）で、
// controller は編集で呼ぶ。行のずれは marks_adjusted が直す。

// 位置を末尾に積む（Vim の setpcmark の後半）。満ちていれば最も古い位置を捨てる。今の位置の番号は
// 末尾の後ろ（count）。同じ行の位置はここでは消さず、歩く前の vim_jumps_cleaned が消す。
[[nodiscard]] VimJumpList vim_jump_appended(const VimJumpList &jumps,
                                            const VimMarkPosition &at) noexcept;

// 同じ行の位置を後ろの 1 つだけ残して消す（Vim の cleanup_jumplist・jumpoptions は空）。今の
// 位置の番号は消えた位置なら次に残る位置へ、末尾の後ろなら新しい末尾の後ろへ。
[[nodiscard]] VimJumpList vim_jumps_cleaned(const VimJumpList &jumps) noexcept;

// `Ctrl-o`（older）と `Ctrl-i`（newer）を count 回（Vim の movemark）。列を片付け、末尾の後ろに
// いれば今の位置（caret）を飛ぶ鍵と同じに積んでから（`''` も置く）、番号を動かす。
[[nodiscard]] VimJumpMove vim_jump_moved(const VimMarks &marks, const VimMarkPosition &caret,
                                         bool older, std::size_t count);

// `g;`（older）と `g,`（newer）を count 回（Vim の get_changelist）。端を越える回数は端で止まり、
// すでに端にいるときだけ失敗する。
[[nodiscard]] VimJumpMove vim_change_moved(const VimMarks &marks, bool older, std::size_t count);

// 変更の位置 at を変更の列に置く（Vim の changed_common）。started は新しい undo の単位の最初の
// 編集か。新しい単位が始まってからまだ足していなくて、最後の位置と違う行か 79 桁より離れていれば
// 新しく足し、どちらでも最後の位置を at にして、今の位置の番号を末尾の後ろへ置く。
[[nodiscard]] VimMarks vim_change_recorded(const VimMarks &marks, const VimMarkPosition &at,
                                           bool started);
} // namespace nenenib::core
