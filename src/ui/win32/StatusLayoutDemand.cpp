#include "StatusLayoutDemand.hpp"

#include "LayoutRect.hpp"
#include "StatusGuideLayout.hpp"

#include <utility>
#include <variant>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] std::size_t left_demand(StatusLeftState left) noexcept
{
    switch (left)
    {
    case StatusLeftState::command_input:
        return 0;
    case StatusLeftState::message:
        return 1;
    case StatusLeftState::mode:
        return 3;
    case StatusLeftState::recording:
        return 4;
    }
    std::unreachable();
}
} // namespace

StatusLeftState status_left_state(const application::EditorFrame &frame) noexcept
{
    const bool palette = frame.command_palette.has_value();
    if (frame.command_line.has_value() && !palette)
    {
        return StatusLeftState::command_input;
    }
    if (frame.command_message.has_value() && !palette)
    {
        return StatusLeftState::message;
    }
    return frame.recording.has_value() ? StatusLeftState::recording : StatusLeftState::mode;
}

std::size_t status_layout_demand(StatusLeftState left, const core::StatusBarLayout &status,
                                 const core::OperationGuideLayout &guide) noexcept
{
    std::size_t items = 0;
    for (const auto &item : status.items)
    {
        items += core::width_of(item) > 0 ? 1 : 0;
    }
    const auto *const shown = std::get_if<core::StatusGuideLayout>(&guide);
    const std::size_t guide_demand = shown == nullptr ? 0 : shown->rows.size() * 2;
    return left_demand(left) + items + guide_demand;
}
} // namespace nenenib::ui::win32
