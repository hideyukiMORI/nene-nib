#pragma once

#include "VimCharacterExtent.hpp"
#include "VimLineExtent.hpp"

#include <variant>

namespace nenenib::core
{
// VISUAL の種類ごとの「範囲の大きさ」の閉じた和型（ADR 0033 の決定 1 / CPP-006）。
// 選択肢が増えたら（`Ctrl-v` の矩形）写し先の足りない std::visit がコンパイルで落ちる（CPP-002）。
using VimVisualExtent = std::variant<VimCharacterExtent, VimLineExtent>;
} // namespace nenenib::core
