#pragma once

#include <cstdint>

namespace nenenib::core
{
// 削除の向き。Backspace が backward、Delete が forward。
enum class DeleteDirection : std::uint8_t
{
    backward,
    forward
};
} // namespace nenenib::core
