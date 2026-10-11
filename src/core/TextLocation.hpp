#pragma once

#include <cstddef>

namespace nenenib::core
{
// 1論理行の中の画面行（0始まり）と、その行の左端からの物理画素。
struct TextLocation
{
    std::size_t row;
    float x;
};
} // namespace nenenib::core
