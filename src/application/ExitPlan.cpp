#include "ExitPlan.hpp"

#include <utility>

namespace nenenib::application
{
ExitPlan exit_plan(ExitCause cause, std::size_t unsaved_tabs) noexcept
{
    // 確かめるものが無ければ、どの経路もすぐ終わってよい。
    if (unsaved_tabs == 0)
    {
        return ExitPlan::leave_now;
    }
    switch (cause)
    {
    case ExitCause::user_close:
        return ExitPlan::confirm_each_tab;
    case ExitCause::rendering_lost:
        return ExitPlan::confirm_each_tab_without_cancel;
    case ExitCause::session_end:
        return ExitPlan::block_session_end;
    }
    std::unreachable();
}
} // namespace nenenib::application
