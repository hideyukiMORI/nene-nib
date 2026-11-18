#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// 補完の面の候補を確定する（通常モードの Enter と Tab・面の行のクリック・ADR 0190 の決定 5）。
// row は確定する候補（今の接頭辞に当たる候補の中の位置・CompletionView の first + 行）で、値なしは
// 選んでいる候補（鍵）。本文に入れるのは 1 編集（undo の 1 単位）で、面を閉じる。面が見えて
// いなければ何もしない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ChooseCompletion
{
    std::optional<std::size_t> row = std::nullopt;
};
} // namespace nenenib::application
