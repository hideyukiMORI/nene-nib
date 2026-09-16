#pragma once

#include <cstdint>

namespace nenenib::core
{
// j / k が戻りたい先の種類（Vim の curswant）。$ は桁ではなく「行末」を貼り付ける。
enum class VimColumnWish : std::uint8_t
{
    at_column,
    at_line_end
};
} // namespace nenenib::core
