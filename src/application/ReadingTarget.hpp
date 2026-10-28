#pragma once

#include "AutosaveId.hpp"
#include "OpenFailureReply.hpp"

#include <variant>

namespace nenenib::application
{
// 裏の読みが揃ったときの行き先（ADR 0159 の決定 4・追補 1）。開く要求は開けなかったときの告げ方を
// 持ち、まだ読んでいないタブの読みはそのタブの退避番号を持つ（帯の位置では名指さない・ADR 0137）。
// 閉じた和型で、中身を読む所は std::visit（CPP-002）。
using ReadingTarget = std::variant<OpenFailureReply, AutosaveId>;
} // namespace nenenib::application
