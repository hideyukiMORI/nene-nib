#pragma once

#include "SettingsControl.hpp"
#include "SettingsSection.hpp"

namespace nenenib::core
{
// 配置の入力の 1 行（#1836）。節（見出しを置く変わり目）と右端の部品。
struct SettingsLayoutRow
{
    SettingsSection section;
    SettingsControl control;
};
} // namespace nenenib::core
