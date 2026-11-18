#pragma once

#include "AutosaveId.hpp"
#include "CompletionCandidate.hpp"
#include "CompletionEntry.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 開いている補完の面の状態（ADR 0190 の決定 1・5・ADR 0169 の決定 3）。窓に 1 つで、持つのは
// controller（候補の列は大きいので、with_* のたびに写る EditorState には置かない・右クリックの
// メニューと同じ形）。
// - id は通し番号（LSP の要求の文脈 LspCompletionContext.session と比べる）、document は開いた
//   文書の退避番号、origin は語の頭（接頭辞の始まり・確定で置き換える範囲の始まり）。
// - by_hand は手で開いたこと（自動で開いた面は LSP の候補が当たるまで出さない）。
// - caret と revision は最後に絞ったときのキャレットと本文の改訂（変わったら絞り直す）。
// - server は LSP の候補（core::completion_entries_of の順）、words は本文の語（Vim の `complete`
//   の順）、words_collected は words を集めたこと。集めるのは面を開いたときと `isIncomplete` の
//   出し直しのときだけで、打っている間は持っている列を接頭辞で絞る（本文を打鍵ごとに走らない）。
// - entries は server ＋ words の列（同じ綴りは LSP の 1 つ）、shown は今の接頭辞に当たる entries
// の
//   位置、selected は shown の中の選んでいる行、first は面の窓の先頭（shown の中の位置）、detailed
//   は当たっている候補のどれかに補足があること。
// - awaiting は LSP の答えを待っていること、incomplete は最後の答えが `isIncomplete` だったこと
//   （次の打鍵で出し直す）、stepped は使う人が選ぶ行を動かしたこと（答えが届いたときに選んでいる
//   候補を保つ）。
// 公開 aggregate（CPP-003）。
struct CompletionSession
{
    std::uint64_t id;
    AutosaveId document;
    core::Offset origin;
    bool by_hand;
    core::Offset caret;
    std::size_t revision;
    std::vector<core::CompletionEntry> server = {};
    std::vector<core::CompletionCandidate> words = {};
    bool words_collected = false;
    std::vector<core::CompletionEntry> entries = {};
    std::vector<std::size_t> shown = {};
    std::size_t selected = 0;
    std::size_t first = 0;
    bool detailed = false;
    bool awaiting = false;
    bool incomplete = false;
    bool stepped = false;
};
} // namespace nenenib::application
