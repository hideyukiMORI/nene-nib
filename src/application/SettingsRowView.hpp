#pragma once

#include "CostTier.hpp"
#include "SettingsRow.hpp"
#include "UiMessage.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::application
{
// 設定の面の 1 行の表示値（追補 2・3）。row は表の行（節と指すもの）、title と description は文の
// 名前（ui は frame.language で引く）、set_name は `:set` の名前。スイッチの行は on と switchable
// （切れない機能は false・ui は「常にオン」と描く）、値の行は value に表示の文字（スイッチの行は
// 空）。tier は測った数字のある機能だけが持ち、background は「裏」の印。
struct SettingsRowView
{
    core::SettingsRow row;
    core::UiMessage title;
    core::UiMessage description;
    std::string_view set_name;
    bool on;
    bool switchable;
    std::string value;
    std::optional<core::CostTier> tier;
    bool background;
};
} // namespace nenenib::application
