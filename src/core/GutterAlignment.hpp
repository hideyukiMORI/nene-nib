#pragma once

#include <cstdint>

namespace nenenib::core
{
// 行番号の欄の中の寄せ（#1041）。既定は右寄せで、`relativenumber` のキャレットの行の絶対の
// 番号だけが左寄せ（Vim の `number` と `relativenumber` の両方の形）。
enum class GutterAlignment : std::uint8_t
{
    trailing,
    leading
};
} // namespace nenenib::core
