#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
enum class PreviewCaretWorkload : std::uint8_t
{
    long_thirty,
    long_full,
    short_thirty,
    confirmed,
    unsearched,
    absent,
    disabled
};
} // namespace nenenib::tests::performance
