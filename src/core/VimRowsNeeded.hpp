#pragma once

#include "LineNumber.hpp"

namespace nenenib::core
{
// vim_topline_updated の「この行の画面行の数が要る」という問い（#1178・ADR 0019 の追補・ADR 0129
// の要求と同じ形）。application はこの行だけを文字組みして VimToplineWindow.rows に足し、呼び直す。
struct VimRowsNeeded
{
    LineNumber line;
};
} // namespace nenenib::core
