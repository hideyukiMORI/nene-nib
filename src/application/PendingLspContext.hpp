#pragma once

#include "LspCompletionContext.hpp"
#include "LspRequestContext.hpp"

#include <variant>

namespace nenenib::application
{
// 答えを待つ要求の文脈の閉じた和型（ADR 0190 の決定 1）。initialize と shutdown は文脈なし
// （monostate）、hover と定義へは LspRequestContext、補完は LspCompletionContext。
using PendingLspContext = std::variant<std::monostate, LspRequestContext, LspCompletionContext>;
} // namespace nenenib::application
