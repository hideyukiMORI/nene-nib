#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeSourcePhase : std::uint8_t
{
    copying,
    ready,
    handed_off,
    cancelled,
    failed
};
}
