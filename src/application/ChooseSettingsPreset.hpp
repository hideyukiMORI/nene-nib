#pragma once

#include "SettingsPreset.hpp"

namespace nenenib::application
{
// 設定の面の組のボタン（ADR 0170 の決定 5・追補 3）。段の印のある切れる機能だけを組の規則で置く。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ChooseSettingsPreset
{
    core::SettingsPreset preset;
};
} // namespace nenenib::application
