#pragma once

#include <cstdint>

namespace nenenib::application
{
// 補完の面の選んでいる行の動かし方（ADR 0190 の決定 5）。上下は前・次（回り込む）、PageUp /
// PageDown は面の行数ぶん（端で止まる）。Ctrl+P の面の鍵と同じ形。
enum class CompletionStep : std::uint8_t
{
    previous,
    next,
    page_previous,
    page_next
};
} // namespace nenenib::application
