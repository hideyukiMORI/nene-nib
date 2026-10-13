#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeIoFault : std::uint8_t
{
    normal,
    short_io,
    zero,
    failed
};
}
