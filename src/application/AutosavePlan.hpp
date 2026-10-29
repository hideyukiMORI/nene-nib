#pragma once

#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosaveStep.hpp"
#include "EditorState.hpp"
#include "EndSession.hpp"

#include <utility>
#include <vector>

namespace nenenib::application
{
// 1 回の AutosaveTick の計画（ADR 0137 の決定 2）。純関数で、帯の順の各タブの仕事と、帳面に
// 残っていて状態に無い（閉じた）タブの discard を返す。
// store: 読み込み済み・未保存・改訂番号が帳面と違う（帳面に無いも含む）。
// discard: 帳面にあって、保存済み・まだ読んでいない・閉じた。
// oversized: store に当たるが本文が core::maximum_decoded_document_bytes を超える（ADR 0159 の
// 決定 7・L-4）。書かず、前の退避も消さない。
// keep: それ以外（未保存で変わっていない・保存済みで帳面に無い・まだ読んでいなくて帳面に無い）。
[[nodiscard]] std::vector<std::pair<AutosaveId, AutosaveStep>>
autosave_plan(const EditorState &state, const AutosaveLedger &ledger);

// 窓が閉じていくとき（EndSession）に退避を消してよいか（ADR 0137 の決定 5）。読み込み済みの
// タブに未保存が 1 つも無いか、使う人が「保存しますか」に「いいえ」と答えて閉じた
// （intent.unsaved_discarded）ときだけ真。確かめずに未保存が残る終わり方（OS の終了の強行）
// では偽で、退避は残骸として次の起動で問われる。
[[nodiscard]] bool clears_autosave(const EditorState &state, const EndSession &intent);
} // namespace nenenib::application
