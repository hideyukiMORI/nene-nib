#pragma once

#include "EasyMotionKind.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimPrefix.hpp"
#include "VimTextObjectScope.hpp"

#include <variant>

namespace nenenib::core
{
// 1 鍵だけ待つ入力状態。文字検索と命令接頭辞とテキストオブジェクトの i / a は同時に成立しない
// （ADR 0027 / ADR 0029 / ADR 0031 の決定 1）。写し先が足りなければ visit がコンパイルで落ちる。
// EasyMotionKind はイージーモーションの次の 1 鍵で、VimState.easy_motion が無ければ f F s の文字、
// あれば印の鍵（ADR 0163 の決定 4・5）。
using VimInputWait =
    std::variant<VimCharacterSearchKind, VimPrefix, VimTextObjectScope, EasyMotionKind>;
} // namespace nenenib::core
