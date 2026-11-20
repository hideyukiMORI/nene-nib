#pragma once

#include <cstdint>

namespace nenenib::application
{
// もう 1 つの枠だけを縦に送る（ADR 0148 の決定 11）。正が下、負が上。枠は移らない。parts は
// ScrollLines と同じ 1 行に満たない量（#1898）。
struct ScrollPane
{
    std::int32_t lines;
    std::int32_t parts = 0;
};
} // namespace nenenib::application
