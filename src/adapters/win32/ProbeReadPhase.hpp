#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeReadPhase : std::uint8_t
{
    queued,
    issuing,
    pending,
    published
};
}
