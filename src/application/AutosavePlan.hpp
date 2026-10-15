#pragma once

#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosaveStep.hpp"
#include "EditorState.hpp"

#include <utility>
#include <vector>

namespace nenenib::application
{
// 1 回の AutosaveTick の計画（ADR 0137 の決定 2）。純関数で、帯の順の各タブの仕事と、帳面に
// 残っていて状態に無い（閉じた）タブの discard を返す。
// store: 読み込み済み・未保存・改訂番号が帳面と違う（帳面に無いも含む）。
// discard: 帳面にあって、保存済み・まだ読んでいない・閉じた。
// keep: それ以外（未保存で変わっていない・保存済みで帳面に無い・まだ読んでいなくて帳面に無い）。
[[nodiscard]] std::vector<std::pair<AutosaveId, AutosaveStep>>
autosave_plan(const EditorState &state, const AutosaveLedger &ledger);
} // namespace nenenib::application
