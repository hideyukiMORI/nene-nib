#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeSourceFailure : std::uint8_t
{
    unreadable,
    unwritable,
    too_large,
    already_exists,
    create_failed,
    unavailable,
    cancelled,
    not_ready,
    consumed,
    invalid_range
};
}
