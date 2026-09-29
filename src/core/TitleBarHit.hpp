#pragma once

#include <cstdint>

namespace nenenib::core
{
// 自前のタイトルバーで当たる部位の閉じた一覧。窓手続きはこれを HTCAPTION 等へ写すだけ（CPP-017）。
// tab_close はタブの × の領域、tab_list はあふれたときの「∨」（ADR 0056 の決定 9）。
enum class TitleBarHit : std::uint8_t
{
    caption,
    tab,
    tab_close,
    add_tab,
    tab_list,
    minimize,
    maximize,
    close,
    none
};
} // namespace nenenib::core
