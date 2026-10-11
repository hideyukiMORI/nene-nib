#pragma once

#include <cstdint>

namespace nenenib::core
{
// 同じ本文の位置が折り返しの両側にあるとき、どちらの画面行にキャレットを置くか。
enum class CaretAffinity : std::uint8_t
{
    leading,
    trailing
};
} // namespace nenenib::core
