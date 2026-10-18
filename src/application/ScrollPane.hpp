#pragma once

#include <cstdint>

namespace nenenib::application
{
// もう 1 つの枠だけを縦に送る（ADR 0148 の決定 11）。正が下、負が上。枠は移らない。
struct ScrollPane
{
    std::int32_t lines;
};
} // namespace nenenib::application
