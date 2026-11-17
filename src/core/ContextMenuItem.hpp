#pragma once

#include "EditorOperation.hpp"

namespace nenenib::core
{
// メニューの 1 行（ADR 0078 の追補 #1051）。separated は行の上に区切り線があること。
struct ContextMenuItem
{
    EditorOperation operation;
    bool enabled;
    bool separated;
};
} // namespace nenenib::core
