#pragma once

#include "ClipboardOperation.hpp"

namespace nenenib::application
{
// Ctrl+C / Ctrl+X / Ctrl+V。実際の OS 資源はポートの向こうにある（ADR 0009 の決定 5）。
struct ClipboardAction
{
    ClipboardOperation operation;
};
} // namespace nenenib::application
