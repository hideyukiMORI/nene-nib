#pragma once

#include "LspCompletionContext.hpp"
#include "LspEditContext.hpp"
#include "LspRequestContext.hpp"

#include <variant>

namespace nenenib::application
{
// 答えを待つ要求の文脈の閉じた和型（ADR 0190 の決定 1）。initialize と shutdown は文脈なし
// （monostate）、hover と定義へは LspRequestContext、補完は LspCompletionContext、答えが
// `WorkspaceEdit` の要求（名前の変更・#2004）は LspEditContext。
using PendingLspContext =
    std::variant<std::monostate, LspRequestContext, LspCompletionContext, LspEditContext>;
} // namespace nenenib::application
