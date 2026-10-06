#pragma once

#include "EditorOperation.hpp"
#include "KeyChord.hpp"
#include "OperationModes.hpp"

#include <optional>

namespace nenenib::core
{
// 割り当ての表の 1 行（ADR 0078 の決定 3・CPP-012）。鍵の無い行は一覧からだけ実行できる操作。
struct OperationBinding
{
    EditorOperation operation;
    OperationModes modes;
    std::optional<KeyChord> chord;
};
} // namespace nenenib::core
