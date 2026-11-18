#pragma once

#include "SettingsSection.hpp"

namespace nenenib::application
{
// 節の札を押す（施主決定 D102 の右の引き出し）。焦点をその節の最初の見えている行へ送る。見えて
// いる行の無い節なら何もしない。使う人の操作なので keeps_tab_walk にも begin_intent の表にも
// 書かない。
struct JumpSettingsSection
{
    core::SettingsSection section;
};
} // namespace nenenib::application
