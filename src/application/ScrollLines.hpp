#pragma once

#include <cstdint>

namespace nenenib::application
{
// ホイールなどによる縦スクロール。正が下、負が上（ADR 0009 の決定 6）。
struct ScrollLines
{
    std::int32_t lines;
};
} // namespace nenenib::application
