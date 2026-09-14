#pragma once

#include <cstdint>

namespace nenenib::core
{
// 表示する 1 行に選択の面があるか。std::optional を表示値に持たないための閉じた型（CPP-004）。
enum class SelectionPresence : std::uint8_t
{
    absent,
    present
};
} // namespace nenenib::core
