#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeReplyKind : std::uint8_t
{
    immediate,
    pending,
    complete,
    incomplete,
    failed,
    not_issued
};
}
