#pragma once

#include "PaneView.hpp"
#include "PreviewView.hpp"

#include <variant>

namespace nenenib::application
{
// もう 1 つの枠に置くものの閉じた和型（ADR 0155 の決定 1・ADR 0148 の決定 2）。受け身の本文の
// 表示 PaneView か、Markdown のプレビュー PreviewView。読む所は全部 std::visit で、選択肢が
// 増えたらコンパイルが落ちる（CPP-002）。
using OtherView = std::variant<PaneView, PreviewView>;
} // namespace nenenib::application
