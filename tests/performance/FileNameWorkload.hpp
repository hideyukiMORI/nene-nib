#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
enum class FileNameWorkload : std::uint8_t
{
    windows,
    deep_ascii,
    mixed_utf8,
    bare_short,
    bare_long,
    trailing,
    root
};
} // namespace nenenib::tests::performance
