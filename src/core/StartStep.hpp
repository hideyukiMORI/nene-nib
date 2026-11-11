#pragma once

#include <cstdint>

namespace nenenib::core
{
// 起動画面の選びの向き（ADR 0172 の追補 2026-11-11 の 5）。next は ↓ と Vim の NORMAL の `j`、
// previous は ↑ と `k`。
enum class StartStep : std::uint8_t
{
    next,
    previous
};
} // namespace nenenib::core
