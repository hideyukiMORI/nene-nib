#pragma once

#include "DeliveryFacts.hpp"
#include "DeliveryStep.hpp"
#include "SettingsInput.hpp"

#include <array>
#include <utility>

namespace nenenib::ui::win32
{
// 意図を送った後に窓が後を追う段の順（#1854・ClosingSteps の形・#1340）。変換の取り消しは IME を
// 開け閉めする follow_ime より前（入力行が先、設定の面が次）。
inline constexpr std::array delivery_order{DeliveryStep::cancel_command_composition,
                                           DeliveryStep::cancel_settings_composition,
                                           DeliveryStep::follow_key_map_timer,
                                           DeliveryStep::follow_tab_tip_timer,
                                           DeliveryStep::announce_font_size,
                                           DeliveryStep::follow_look,
                                           DeliveryStep::follow_split,
                                           DeliveryStep::follow_explorer,
                                           DeliveryStep::follow_minimap,
                                           DeliveryStep::follow_ime};

// この意図の後に段が要るか。条件のある段は 3 つで、ほかは毎回。
[[nodiscard]] constexpr bool delivery_step_due(DeliveryStep step,
                                               const DeliveryFacts &facts) noexcept
{
    switch (step)
    {
    case DeliveryStep::cancel_command_composition:
        return facts.command_composed && !facts.command_composing && !facts.composition_settled;
    case DeliveryStep::cancel_settings_composition:
        return settings_just_closed(facts.settings_was_open, facts.settings_open);
    case DeliveryStep::announce_font_size:
        return facts.font_size_adjusted;
    case DeliveryStep::follow_key_map_timer:
    case DeliveryStep::follow_tab_tip_timer:
    case DeliveryStep::follow_look:
    case DeliveryStep::follow_split:
    case DeliveryStep::follow_explorer:
    case DeliveryStep::follow_minimap:
    case DeliveryStep::follow_ime:
        return true;
    }
    std::unreachable();
}

// 要る段を順に呼ぶ唯一の口。EditorWindow::deliver が段の写しを渡し、試験は替え玉で呼ばれた段を
// 数える。
template <typename Runner>
constexpr void run_delivery_steps(const DeliveryFacts &facts, const Runner &run)
{
    for (const DeliveryStep step : delivery_order)
    {
        if (delivery_step_due(step, facts))
        {
            run(step);
        }
    }
}
} // namespace nenenib::ui::win32
