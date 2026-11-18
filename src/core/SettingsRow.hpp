#pragma once

#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"

namespace nenenib::core
{
// 設定の面の表の 1 行（節と指すもの）。表は settings_rows の 1 つ。
struct SettingsRow
{
    SettingsSection section;
    SettingsSubject subject;
};
} // namespace nenenib::core
