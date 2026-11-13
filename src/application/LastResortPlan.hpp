#pragma once

#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosaveStep.hpp"
#include "EditorState.hpp"
#include "IntentMark.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace nenenib::application
{
// 意図を写し始めるときの印（apply の入口・いちばん外の意図だけ）。
[[nodiscard]] IntentMark intent_mark_of(const EditorState &state);

// 最後の砦の計画（ADR 0137 の追補 #1625）。捕まえていない例外で落ちる直前に、決定 9 と同じ
// 退避を 1 回だけ回すときの仕事。depth は写している途中の意図の深さ（落ちたときは apply の出口を
// 通らないので 0 より大きいまま）。
// - depth が 0（意図の外で落ちた）: autosave_plan そのまま。
// - 意図の中で、アクティブのタブ・タブの数・切り替えの回数が入口と同じ（アクティブ以外のタブには
//   触れていない）: アクティブのタブの改訂番号が入口と同じなら autosave_plan
//   そのまま。進んでいれば、 そのタブだけ keep
//   にする（途中の本文で前の退避を上書きしない）。ほかのタブは autosave_plan のまま。
// - 意図の中で、どれかが入口と違う（行って戻った切り替えを含む）: 何もしない（空の計画・前の退避を
//   残す）。
[[nodiscard]] std::vector<std::pair<AutosaveId, AutosaveStep>>
last_resort_plan(const EditorState &state, const AutosaveLedger &ledger, const IntentMark &entered,
                 std::size_t depth);
} // namespace nenenib::application
