#pragma once

#include <cstdint>

namespace nenenib::application
{
// 窓が終わっていく経路（ADR 0133 の決定 2）。user_close は使う人が閉じる（×・Alt+F4・WM_CLOSE）、
// rendering_lost は描画を失った（作り直しの 1 回も失敗した）、session_end は OS が終わる
// （WM_QUERYENDSESSION）。値が増えたら exit_plan の `switch` が落ちる（CPP-002）。
enum class ExitCause : std::uint8_t
{
    user_close,
    rendering_lost,
    session_end
};
} // namespace nenenib::application
