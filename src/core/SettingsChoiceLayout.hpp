#pragma once

#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 選ぶ欄の一覧の配置（#1926）。panel は一覧の矩形、first は描く最初の選択肢の位置（送りを
// 切り詰めた値）、rows は描く行の数、count は選択肢の数（rows より多ければ送りの棒を
// 描く）、above は値の箱の上に出したこと。行の矩形は settings_choice_row が panel から
// 引く。公開 aggregate（CPP-003）。
struct SettingsChoiceLayout
{
    LayoutRect panel;
    std::size_t first;
    std::size_t rows;
    std::size_t count;
    bool above;
    std::uint32_t dpi;
};
} // namespace nenenib::core
