#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeReadFailure : std::uint8_t
{
    invalid_count,
    create_failed,
    consumed,
    not_ready
};
}
