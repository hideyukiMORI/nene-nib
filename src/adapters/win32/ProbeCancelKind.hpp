#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeCancelKind : std::uint8_t
{
    accepted,
    already_requested,
    too_late
};
}
