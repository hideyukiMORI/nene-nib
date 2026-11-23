#pragma once

#include <cstddef>

namespace nenenib::core
{
// 強める段の区間（ADR 0193 の決定 9）。見えている行の列の中の [begin, end) の行で、段 level
// （0 始まり・桁 level × 段の幅の線）を強める。公開 aggregate（CPP-003）。
struct IndentGuideRun
{
    std::size_t begin;
    std::size_t end;
    std::size_t level;
};
} // namespace nenenib::core
