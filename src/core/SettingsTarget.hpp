#pragma once

#include "SettingsPart.hpp"

#include <cstddef>

namespace nenenib::core
{
// 設定の面の当たり（#1836）。index は札・組・行の番号（行は見えている行の中の位置）で、ほかの
// 部品では 0。押した要素と離した要素が同じかを比べる（ADR 0056 の決定 9 と同じ考え）。
struct SettingsTarget
{
    SettingsPart part;
    std::size_t index;
};

[[nodiscard]] constexpr bool operator==(const SettingsTarget &left,
                                        const SettingsTarget &right) noexcept
{
    return left.part == right.part && left.index == right.index;
}
} // namespace nenenib::core
