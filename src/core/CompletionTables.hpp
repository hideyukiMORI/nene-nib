#pragma once

#include "WordTable.hpp"

#include <span>

namespace nenenib::core
{
// 候補を集める語の表（ADR 0169 の決定 1・2）。own は今の文書の表（作っていない文書は nullptr・
// 16 MiB 超）、others は読み込み済みの他のタブの表（帯の順）。指す先は呼ぶ間だけ借りる。
struct CompletionTables
{
    const WordTable *own = nullptr;
    std::span<const WordTable *const> others{};
};
} // namespace nenenib::core
