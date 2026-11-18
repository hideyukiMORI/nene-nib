#pragma once

namespace nenenib::application
{
// 補完の面を閉じる（Esc と窓の非アクティブ化・ADR 0190 の決定 5）。本文はそのまま。答えを待って
// いる要求は取り消し、届いていた補完の答えを消す。使う人の操作なので keeps_tab_walk にも
// begin_intent の表にも書かない。
struct CloseCompletion
{
};
} // namespace nenenib::application
