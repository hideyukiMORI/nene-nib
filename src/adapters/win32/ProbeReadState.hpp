#pragma once
#include <cstdint>
namespace nenenib::adapters::win32
{
enum class ProbeReadState : std::uint8_t
{
    pending,
    cancelled,
    consumed
};
}
