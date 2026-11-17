#pragma once

#include "EasyMotionCancel.hpp"
#include "EasyMotionJump.hpp"
#include "EasyMotionNarrowed.hpp"

#include <variant>

namespace nenenib::core
{
// 印の鍵 1 つの答えの閉じた和型（ADR 0163 の決定 5）。絞る・跳ぶ・取消のどれかで、選択肢が
// 増えたら engine の std::visit の写し先が足りずコンパイルが落ちる（CPP-002）。
using EasyMotionOutcome = std::variant<EasyMotionNarrowed, EasyMotionJump, EasyMotionCancel>;
} // namespace nenenib::core
