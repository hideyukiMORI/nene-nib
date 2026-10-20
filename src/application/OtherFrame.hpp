#pragma once

#include "PaneFrame.hpp"
#include "PreviewFrame.hpp"

#include <variant>

namespace nenenib::application
{
// もう 1 つの枠の表示値の閉じた和型（ADR 0155 の決定 1・11）。OtherView の選択肢と 1 対 1 で、
// 描く所は std::visit で読む。
using OtherFrame = std::variant<PaneFrame, PreviewFrame>;
} // namespace nenenib::application
