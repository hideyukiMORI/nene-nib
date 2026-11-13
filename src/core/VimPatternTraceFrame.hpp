#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 命令の列の道で群の範囲を引く探索（VimPatternTrace）の積みの 1 段。pc の命令を at で試している
// 所で、phase は次に試す枝（0 は入った所・1 は split の後の道・2 は済み）。値は単独で妥当なので
// 公開 aggregate（CPP-003）。
struct VimPatternTraceFrame
{
    std::size_t at;
    std::size_t pc;
    std::uint8_t phase;
};
} // namespace nenenib::core
