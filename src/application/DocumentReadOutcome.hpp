#pragma once

#include "DocumentReadFinished.hpp"
#include "DocumentReadProgress.hpp"

#include <variant>

namespace nenenib::application
{
// 読みの port が返す値の閉じた和型（ADR 0159 の決定 2・CPP-002）。中身を読む所は std::visit。
using DocumentReadOutcome = std::variant<DocumentReadProgress, DocumentReadFinished>;
} // namespace nenenib::application
