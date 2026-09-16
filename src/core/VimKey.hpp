#pragma once

#include "VimCharacter.hpp"
#include "VimSpecialKey.hpp"

#include <variant>

namespace nenenib::core
{
// Vim モードの鍵の閉じた和型（ADR 0012 の決定 4 / CPP-006）。開いた文字列で受け取ると
// 網羅性が守れないので、窓の側で 1 か所だけこの値に写す。
using VimKey = std::variant<VimCharacter, VimSpecialKey>;
} // namespace nenenib::core
