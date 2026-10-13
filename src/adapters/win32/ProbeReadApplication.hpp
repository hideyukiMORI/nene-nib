#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeReadApplication : std::uint8_t
{
    delivered,
    cancelled,
    failed
};
}
