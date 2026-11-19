#pragma once

#include "SettingsSection.hpp"
#include "UiMessage.hpp"

namespace nenenib::application
{
// 設定の面の節の札（施主決定 D102）。札は 6 つの節を宣言の順に全部並べ、filled は探す欄で絞った
// 後に見えている行がその節にあるか（無い札は ui が淡く描き、押しても動かない）。
struct SettingsSectionTab
{
    core::SettingsSection section;
    core::UiMessage title;
    bool filled;
};
} // namespace nenenib::application
