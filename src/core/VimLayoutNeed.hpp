#pragma once

#include "LineNumber.hpp"
#include "VimRowRequest.hpp"

#include <variant>

namespace nenenib::core
{
// 画面移動の解決が足りない文字組みの値（ADR 0144 の決定 2）。行の画面行数（VimStep.layout_request）
// か、行の中の画面行（VimStep.row_request）のどちらか 1 つ。
using VimLayoutNeed = std::variant<LineNumber, VimRowRequest>;
} // namespace nenenib::core
