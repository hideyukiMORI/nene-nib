#pragma once

#include <cstdint>

namespace nenenib::core
{
// ステータスバーで当たる部位の閉じた一覧。今はモードトグルの 2 つだけ（ADR 0008）。
enum class StatusBarHit : std::uint8_t
{
    toggle_ordinary,
    toggle_vim,
    none
};
} // namespace nenenib::core
