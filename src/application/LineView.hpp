#pragma once

#include "LineNumber.hpp"
#include "SelectionSpan.hpp"

#include <string>

namespace nenenib::application
{
// 見えている 1 行の表示値（ARC-011）。UI はこれを写すだけで本文には触らない。
struct LineView
{
    core::LineNumber number;
    std::string text;
    core::SelectionSpan selection;
};
} // namespace nenenib::application
