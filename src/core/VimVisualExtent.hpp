#pragma once

#include "VimBlockExtent.hpp"
#include "VimCharacterExtent.hpp"
#include "VimLineExtent.hpp"

#include <variant>

namespace nenenib::core
{
// VISUAL の種類ごとの「範囲の大きさ」の閉じた和型（ADR 0033 の決定 1 / ADR 0035 の決定 7 /
// CPP-006）。選択肢が増えたら写し先の足りない std::visit がコンパイルで落ちる（CPP-002）。
using VimVisualExtent = std::variant<VimCharacterExtent, VimLineExtent, VimBlockExtent>;
} // namespace nenenib::core
