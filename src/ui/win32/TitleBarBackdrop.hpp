#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// タイトルバーの地。Mica が掛かったかどうかは DwmSetWindowAttribute の結果で決まり、
// 掛からない環境では本文と同じ地の色で塗る（ADR 0008・例外にしない）。
enum class TitleBarBackdrop : std::uint8_t
{
    mica,
    opaque
};
} // namespace nenenib::ui::win32
