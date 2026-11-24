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
    case StatusLeftState::diagnostic:
        return 4;
    }
    std::unreachable();
}
} // namespace

StatusLeftState status_left_of(const StatusLeftInputs &inputs) noexcept
{
    if (inputs.command_line && !inputs.palette)
    {
        return StatusLeftState::command_input;
    }
    if (inputs.message && !inputs.palette)
    {
        return StatusLeftState::message;
    }
    // 診断の文はトグルとモードを隠さず、モードの文の後ろに書く（ADR 0161 の 2026-11-24 の追補・
    // #2025）。録画中の frame は application が文を載せない（`recording @a` が先・R3-3）。
    if (inputs.diagnostic && !inputs.palette)
    {
        return StatusLeftState::diagnostic;
    }
    return inputs.recording ? StatusLeftState::recording : StatusLeftState::mode;
}

StatusLeftState status_left_state(const application::EditorFrame &frame) noexcept
{
    return status_left_of(StatusLeftInputs{
        .palette = frame.command_palette.has_value(),
        .command_line = frame.command_line.has_value(),
        .message = frame.command_message.has_value(),
        .diagnostic = frame.diagnostic_message.has_value(),
        .recording = frame.recording.has_value(),
    });
}

std::size_t status_layout_demand(StatusLeftState left, const core::StatusBarLayout &status,
                                 const core::OperationGuideLayout &guide) noexcept
{
    std::size_t items = 0;
    for (const auto &item : status.items)
    {
        items += core::width_of(item) > 0 ? 1 : 0;
    }
    items += core::width_of(status.diagnostics) > 0 ? 1 : 0;
    const auto *const shown = std::get_if<core::StatusGuideLayout>(&guide);
    const std::size_t guide_demand = shown == nullptr ? 0 : shown->rows.size() * 2;
    return left_demand(left) + items + guide_demand;
}
} // namespace nenenib::ui::win32
