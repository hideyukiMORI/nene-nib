#pragma once

#include <cstdint>

namespace nenenib::core
{
// 設定の面の当たりの部品（施主決定 D102・#1836）。outside は面の外、panel は面の中の何も無い所。
// chip は節の札・preset は組のボタン・row は行（部品の外）・row_value は選ぶ欄・row_minus と
// row_plus は段の上下。revert と close_button は下の帯のボタン、close は見出しの ✕。
enum class SettingsPart : std::uint8_t
{
    outside,
    panel,
    close,
    search,
    chip,
    preset,
    row,
    row_value,
    row_minus,
    row_plus,
    revert,
    close_button
};
} // namespace nenenib::core
