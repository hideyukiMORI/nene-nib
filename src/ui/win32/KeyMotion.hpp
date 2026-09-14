#pragma once

#include "CaretMotion.hpp"

#include <cstdint>

namespace nenenib::ui::win32
{
// 仮想キー 1 つと、それが表すキャレットの移動。表で引くための 1 行（CPP-012 / ADR 0006）。
// Ctrl の有無は表そのものを分けて表す（bool の制御値を持たない）。
struct KeyMotion
{
    std::uint32_t key;
    core::CaretMotion motion;
};
} // namespace nenenib::ui::win32
