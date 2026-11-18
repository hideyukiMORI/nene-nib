#pragma once

#include <cstdint>

namespace nenenib::core
{
// 表の列の揃え（`:---` 左・`:---:` 中央・`---:` 右・指定なしは左・#1848・D103）。
enum class PreviewTableAlign : std::uint8_t
{
    left,
    center,
    right
};
} // namespace nenenib::core
