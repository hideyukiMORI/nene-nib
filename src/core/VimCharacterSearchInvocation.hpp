#pragma once

#include <cstdint>

namespace nenenib::core
{
// 最初の f/F/t/T と ;/, の反復を分ける閉じた値。t/T の隣接一致の扱いが異なる。
enum class VimCharacterSearchInvocation : std::uint8_t
{
    first,
    repeat
};
} // namespace nenenib::core
