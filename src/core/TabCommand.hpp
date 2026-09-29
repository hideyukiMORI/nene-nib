#pragma once

#include <cstdint>

namespace nenenib::core
{
// タブの鍵が窓へ頼む命令の閉じた一覧（ADR 0056 の決定 10）。open は NewTab、next / previous は
// WalkRecentTab（ADR 0058 の決定 4）、close は閉じる流れ（未保存なら確認・最後の 1 本なら
// 窓を閉じる）へ写す。
enum class TabCommand : std::uint8_t
{
    open,
    next,
    previous,
    close
};
} // namespace nenenib::core
