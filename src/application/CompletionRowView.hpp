#pragma once

#include "CompletionSource.hpp"

#include <string>

namespace nenenib::application
{
// 補完の面の 1 行の表示値（ADR 0169 の決定 4）。label は候補の名前、detail は右に添える補足（LSP の
// `detail`・訳さない・無ければ空）、source は出どころ（ui が core::completion_source_mark の印にする）。
struct CompletionRowView
{
    std::string label;
    std::string detail;
    core::CompletionSource source;
};
} // namespace nenenib::application
