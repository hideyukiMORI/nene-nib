#pragma once

#include "HistoryDirection.hpp"

namespace nenenib::application
{
// Ctrl+Z / Ctrl+Y。履歴の端では何も起きない（ARC-010）。
struct HistoryAction
{
    core::HistoryDirection direction;
};
} // namespace nenenib::application
