#pragma once

#include "CompletionDirection.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace nenenib::core
{
// 開いている INSERT の補完（#1845・ADR 0169 の決定 3・Vim の insexpand.c の compl_* に当たる）。
// words は候補の列（direction の向きの順）で、輪は「候補の列 ＋ 元の接頭辞」。selected は輪の中の
// 位置で、words->size() のとき元の接頭辞（Vim が列の端で元に戻る 1 歩）。begin から今のキャレット
// までの本文が選んでいる語（か元の接頭辞）で、選ぶたびに置き換える。original は補完を始めたときの
// begin からキャレットまでの本文。値は不変で、歩くたびに新しい値を作る（列は共有する）。
struct VimInsertCompletion
{
    std::shared_ptr<const std::vector<std::string>> words;
    std::string original;
    Offset begin;
    std::size_t selected;
    CompletionDirection direction;
};
} // namespace nenenib::core
