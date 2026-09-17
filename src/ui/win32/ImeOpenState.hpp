#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// Vim の NORMAL に入る前の IME の開閉を控える 3 つの値（ADR 0014 の決定 5）。
// unrecorded は「まだ切っていない＝戻すものが無い」で、bool 2 つでも真偽の制御値でもなく、
// この 1 つの閉じた enum が窓の持つ唯一の IME の状態である（CPP-002 / CPP-006）。
enum class ImeOpenState : std::uint8_t
{
    unrecorded,
    open,
    closed
};
} // namespace nenenib::ui::win32
