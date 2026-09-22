#pragma once

#include "VimCharacter.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"

#include <variant>

namespace nenenib::core
{
// Vim モードの鍵の閉じた和型（ADR 0012 の決定 4 / CPP-006）。開いた文字列で受け取ると
// 網羅性が守れないので、窓の側で 1 か所だけこの値に写す。確定した検索だけは窓ではなく
// controller の入力行が作る鍵で、`.` の記録にもこの形で残る（ADR 0032 の決定 3）。
using VimKey = std::variant<VimCharacter, VimSpecialKey, VimSearchPattern>;
} // namespace nenenib::core
