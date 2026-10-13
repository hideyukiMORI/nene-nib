#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeQueryWait : std::uint8_t
{
    poll,
    wait
};
}
