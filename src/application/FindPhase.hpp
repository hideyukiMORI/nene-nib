#pragma once

#include <cstdint>

namespace nenenib::application
{
// 通常モードの検索の流れの段（ADR 0174 の決定 3）。「検索: 」の Enter が何をするかを決める。
// いまは Ctrl+F の流れ（次の当たりへ）だけで、置換の 2 段（Ctrl+H の「検索: 」→「置換後: 」）は
// N1b がここに値を足し、落ちた switch を直す（CPP-002）。
enum class FindPhase : std::uint8_t
{
    // Ctrl+F の「検索: 」。Enter / F3 / Shift+F3 は当たりへ移り、入力行は閉じない（Esc で閉じる）。
    find
};
} // namespace nenenib::application
