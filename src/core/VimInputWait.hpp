#pragma once

#include "VimCharacterSearchKind.hpp"
#include "VimPrefix.hpp"

#include <variant>

namespace nenenib::core
{
// 1 鍵だけ待つ入力状態。文字検索と命令接頭辞は同時に成立しない（ADR 0027）。
using VimInputWait = std::variant<VimCharacterSearchKind, VimPrefix>;
} // namespace nenenib::core
