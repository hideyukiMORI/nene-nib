#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
enum class FrameRowsWorkload : std::uint8_t
{
    frame_rows_empty,
    frame_rows_short,
    frame_rows_thirty,
    frame_rows_full
};
} // namespace nenenib::tests::performance
