#include "PresentedMilestones.hpp"

namespace nenenib::ui::win32
{
void PresentedMilestones::presented(bool display_pending, application::TimingPort &timing) noexcept
{
    timing.mark(core::Milestone::frame_presented);
    if (body_presented_ || display_pending)
    {
        return;
    }
    body_presented_ = true;
    timing.mark(core::Milestone::body_presented);
}
} // namespace nenenib::ui::win32
