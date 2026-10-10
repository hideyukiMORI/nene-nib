#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
enum class FrameDocumentWorkload : std::uint8_t
{
    short_saved,
    long_saved,
    long_failed,
    untitled
};
} // namespace nenenib::tests::performance
