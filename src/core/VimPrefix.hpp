#pragma once

#include <cstdint>

namespace nenenib::core
{
// 次の鍵で完成する NORMAL 命令の閉じた接頭辞（ADR 0027）。
enum class VimPrefix : std::uint8_t
{
    g
};
} // namespace nenenib::core
