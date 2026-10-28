#pragma once

#include "ExplorerTree.hpp"

#include <cstddef>

namespace nenenib::core
{
// h と ← の答え（ADR 0162 の決定 1・4）。tree は閉じた後の木、row は選ぶ行（ExplorerTree::
// collapsed_to_parent の注釈）。
struct ExplorerStep
{
    ExplorerTree tree;
    std::size_t row;
};
} // namespace nenenib::core
