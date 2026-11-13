#pragma once

#include "AutosaveId.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// いちばん外の意図を写し始めたときの印（ADR 0137 の追補 #1625）。アクティブなタブの退避番号・
// その改訂番号（EditHistory::revision）・タブの数・切り替えの回数（EditorState::tab_switches）。
// apply の入口が整数を書くだけで、最後の砦の計画（last_resort_plan）が「意図の途中で動いたタブ」を
// 見分けるのに読む。値だけを運ぶ公開 aggregate（CPP-003）。
struct IntentMark
{
    AutosaveId active{0};
    std::size_t revision = 0;
    std::size_t tabs = 0;
    std::uint64_t switches = 0;
};
} // namespace nenenib::application
