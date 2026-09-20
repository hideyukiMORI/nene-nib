#pragma once

#include <cstdint>

namespace nenenib::core
{
// 次の鍵で完成するNORMAL/VISUAL命令の閉じた接頭辞（ADR 0027 / 0029）。
enum class VimPrefix : std::uint8_t
{
    g,
    r
};
} // namespace nenenib::core
