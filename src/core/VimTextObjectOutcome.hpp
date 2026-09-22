#pragma once

#include "VimTextObjectCancel.hpp"
#include "VimTextObjectSpan.hpp"

#include <variant>

namespace nenenib::core
{
// テキストオブジェクトの答えの閉じた和型（CPP-006 / ADR 0031 の補足）。範囲になったか、
// 取消か。選択肢が増えたら engine の std::visit の写し先が足りずコンパイルが落ちる（CPP-002）。
using VimTextObjectOutcome = std::variant<VimTextObjectSpan, VimTextObjectCancel>;
} // namespace nenenib::core
