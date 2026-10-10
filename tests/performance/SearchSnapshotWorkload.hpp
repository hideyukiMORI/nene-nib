#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
enum class SearchSnapshotWorkload : std::uint8_t
{
    frame_short,
    frame_long,
    repeat_long,
    retained_long,
    retained_short,
    unsearched,
    commit_short,
    typing_frame_short
};
} // namespace nenenib::tests::performance
