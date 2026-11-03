#pragma once

#include "Diagnostic.hpp"
#include "DisplayText.hpp"
#include "Offset.hpp"

#include <optional>
#include <span>

namespace nenenib::core
{
// キャレットの上の診断の文（ADR 0161 の決定 4）。キャレットが診断の区間 [begin, end) の中にある
// （空の区間はその位置にある）診断のうち、いちばん重いもの（同じ重さなら列の先のもの）の文を
// notice_line で 1 行にする。キャレットがどの区間の上にも無ければ値なし。frame
// を作るたびに作る値で、 状態に積まない。
[[nodiscard]] std::optional<DisplayText> diagnostic_notice(std::span<const Diagnostic> diagnostics,
                                                           Offset caret);
} // namespace nenenib::core
