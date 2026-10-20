#include "Win32Bell.hpp"

#include <windows.h>

namespace nenenib::adapters::win32
{
void Win32Bell::ring() noexcept
{
    // 鳴らせなくても知らせの 1 行は出ているので、失敗は捨てる（ADR 0153 の決定 5）。
    static_cast<void>(MessageBeep(MB_ICONEXCLAMATION));
}
} // namespace nenenib::adapters::win32
