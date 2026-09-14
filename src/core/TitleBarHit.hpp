#pragma once

#include <cstdint>

namespace nenenib::core
{
// 自前のタイトルバーで当たる部位の閉じた一覧。窓手続きはこれを HTCAPTION 等へ写すだけ（CPP-017）。
enum class TitleBarHit : std::uint8_t
{
    caption,
    tab,
    add_tab,
    minimize,
    maximize,
    close,
    none
};
} // namespace nenenib::core
