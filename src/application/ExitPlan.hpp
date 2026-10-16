#pragma once

#include "ExitCause.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 終わり方ごとの未保存の編集の扱い（ADR 0133 の決定 2）。ui
// はこの値に従って動くだけで、表を書かない。
enum class ExitPlan : std::uint8_t
{
    // 未保存のタブを順に映して「保存しますか」（保存／捨てる／取消）。取消か保存の失敗で止め、窓は続く。
    confirm_each_tab,
    // 未保存のタブを順に映して「描画できなくなったので閉じます。保存しますか」（保存／捨てる）。
    // 窓は描けないので取消は無い。保存に失敗したら理由を見せてもう一度聞く。
    confirm_each_tab_without_cancel,
    // OS の終了を止めて理由を見せる（ShutdownBlockReasonCreate・#493）。窓は続く。
    block_session_end,
    // 確かめるものが無いので、すぐ終わってよい。
    leave_now
};

// 経路と未保存のタブの数から扱いを決める純関数（ADR 0133 の決定 2）。
[[nodiscard]] ExitPlan exit_plan(ExitCause cause, std::size_t unsaved_tabs) noexcept;
} // namespace nenenib::application
