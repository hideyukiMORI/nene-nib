#pragma once

#include "JsonValue.hpp"
#include "LspPosition.hpp"

namespace nenenib::core
{
// LSP の Position → JSON の `{"line":N,"character":M}`（ADR 0161 の決定 1・ADR 0190 の決定 1）。
// 変更の範囲（didChange）と要求の位置（補完・hover・定義へ）が同じ 1 本で書く（ARC-012）。
[[nodiscard]] JsonValue lsp_position_json(LspPosition position);
} // namespace nenenib::core
