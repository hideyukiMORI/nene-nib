#pragma once

#include "VimCharacterSearchKind.hpp"
#include "VimPrefix.hpp"
#include "VimTextObjectScope.hpp"

#include <variant>

namespace nenenib::core
{
// 1 鍵だけ待つ入力状態。文字検索と命令接頭辞とテキストオブジェクトの i / a は同時に成立しない
// （ADR 0027 / ADR 0029 / ADR 0031 の決定 1）。写し先が足りなければ visit がコンパイルで落ちる。
using VimInputWait = std::variant<VimCharacterSearchKind, VimPrefix, VimTextObjectScope>;
} // namespace nenenib::core
