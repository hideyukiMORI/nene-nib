#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeReadHook : std::uint8_t
{
    issue,
    pending,
    query,
    cancel,
    before_publish,
    finished,
    cancel_return
};
}
