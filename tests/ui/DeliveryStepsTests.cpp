// #1854: 意図を送った後に窓が後を追う段の列（DeliverySteps）。
// 形は ClosingSteps（#1340）と同じで、窓は作らない。
//
// 面を閉じる道は 2 種類ある。
// - CloseSettings を出す 5 つの道: ×・「閉じる」の離し、Esc、Ctrl+,、
//   面の外の左の押し、面の外の右の押し。
// - application の入口 close_settings_before が閉じる意図
//   （帯のタブの操作・EndSession など、面を開いたまま受けない意図）。
// どちらも段の値は deliver の後の controller の settings_open() から取るので、
// 試験の after は道を問わない。前者は窓の手続きと同じ純関数（SettingsInput）に
// 流して意図を得る。閉じた道では変換の取り消しの段が 1 回で、IME を開け閉めする
// follow_ime より前。開いたままの道（面の中の押しと離し・探す欄の打鍵・
// Ctrl+, の長押しの反復）では 0 回。
//
// CloseSettings で面が閉じることは application の `--settings-modal` が見るので、
// ここでは「意図の列に CloseSettings があれば閉じた」と置く。
// 限界: 試験の外に 3 つある。EditorWindow::deliver の run_delivery_steps の呼び出し、
// deliver で facts を組む所、delivery_step の写し（どの段が何を呼ぶか）。
// 実機の IME の確認は施主の試用の項目。
#include "CloseSettings.hpp"
#include "DeliveryFacts.hpp"
#include "DeliveryStep.hpp"
#include "DeliverySteps.hpp"
#include "EditorFeature.hpp"
#include "EditorIntent.hpp"
#include "SettingsInput.hpp"
#include "SettingsKeyPress.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPart.hpp"
#include "SettingsRow.hpp"
#include "SettingsRowView.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsTarget.hpp"
#include "SettingsView.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using nenenib::ui::win32::delivery_order;
using nenenib::ui::win32::DeliveryFacts;
using nenenib::ui::win32::DeliveryStep;
using nenenib::ui::win32::run_delivery_steps;
using nenenib::ui::win32::settings_character_intents;
using nenenib::ui::win32::settings_key_intents;
using nenenib::ui::win32::settings_press_step;
using nenenib::ui::win32::settings_release_intents;
using nenenib::ui::win32::settings_right_press_intents;
using nenenib::ui::win32::SettingsIntents;
using nenenib::ui::win32::SettingsKeyPress;
using Steps = std::vector<DeliveryStep>;

constexpr std::size_t guide_row = 0;

[[nodiscard]] Steps recorded(const DeliveryFacts &facts)
{
    Steps steps;
    run_delivery_steps(facts, [&steps](DeliveryStep step) { steps.push_back(step); });
    return steps;
}

[[nodiscard]] std::size_t count_of(const Steps &steps, DeliveryStep step)
{
    return static_cast<std::size_t>(std::ranges::count(steps, step));
}

[[nodiscard]] std::ptrdiff_t position_of(const Steps &steps, DeliveryStep step)
{
    return std::distance(steps.begin(), std::ranges::find(steps, step));
}

// 面を描く前の配置（無い）。
[[nodiscard]] const std::optional<core::SettingsLayout> &undrawn()
{
    static const std::optional<core::SettingsLayout> none;
    return none;
}

// 見本の面: 操作の案内（オフのスイッチ）の 1 行に焦点。
[[nodiscard]] app::SettingsView sample()
{
    const core::SettingsSubject subject = core::EditorFeature::guide;
    app::SettingsView view{};
    view.rows.push_back(
        app::SettingsRowView{core::SettingsRow{core::SettingsSection::display, subject},
                             core::settings_row_title(subject),
                             core::settings_row_description(subject),
                             core::settings_row_set_name(subject),
                             false,
                             true,
                             {},
                             std::nullopt,
                             false});
    view.focused = guide_row;
    return view;
}

// 開いている面に道の意図を送った後の段の列。application は CloseSettings で面を閉じる。
[[nodiscard]] Steps after(const SettingsIntents &intents)
{
    const bool closes =
        std::ranges::any_of(intents, [](const app::EditorIntent &intent)
                            { return std::holds_alternative<app::CloseSettings>(intent); });
    return recorded(DeliveryFacts{.settings_was_open = true, .settings_open = !closes});
}

// 閉じた道: 取り消しの段が 1 回で、follow_ime より前。入力行の取り消しは混ざらない。
[[nodiscard]] bool cancels_once(const Steps &steps)
{
    return count_of(steps, DeliveryStep::cancel_settings_composition) == 1 &&
           position_of(steps, DeliveryStep::cancel_settings_composition) <
               position_of(steps, DeliveryStep::follow_ime) &&
           count_of(steps, DeliveryStep::follow_ime) == 1 &&
           count_of(steps, DeliveryStep::cancel_command_composition) == 0;
}

[[nodiscard]] bool leaves_ime(const Steps &steps)
{
    return count_of(steps, DeliveryStep::cancel_settings_composition) == 0 &&
           count_of(steps, DeliveryStep::follow_ime) == 1;
}

[[nodiscard]] SettingsIntents key(WPARAM code, bool control, bool repeated)
{
    return settings_key_intents(SettingsKeyPress{code, control, false, repeated}, sample(),
                                undrawn());
}

[[nodiscard]] SettingsIntents clicked(core::SettingsPart part)
{
    const core::SettingsTarget target{part, 0};
    const auto [pressed, intents] = settings_press_step(target);
    auto released = settings_release_intents(pressed, target, sample());
    released.insert(released.begin(), intents.begin(), intents.end());
    return released;
}

void verify_order()
{
    constexpr std::array expected{DeliveryStep::cancel_command_composition,
                                  DeliveryStep::cancel_settings_composition,
                                  DeliveryStep::follow_key_map_timer,
                                  DeliveryStep::follow_tab_tip_timer,
                                  DeliveryStep::announce_font_size,
                                  DeliveryStep::follow_look,
                                  DeliveryStep::follow_split,
                                  DeliveryStep::follow_explorer,
                                  DeliveryStep::follow_minimap,
                                  DeliveryStep::follow_ime};
    expect(std::ranges::equal(delivery_order, expected),
           "the steps after an intent cancel compositions first and open or close the IME last");
    const auto quiet = recorded(DeliveryFacts{});
    expect(quiet.size() == expected.size() - 3 &&
               count_of(quiet, DeliveryStep::cancel_command_composition) == 0 &&
               count_of(quiet, DeliveryStep::cancel_settings_composition) == 0 &&
               count_of(quiet, DeliveryStep::announce_font_size) == 0,
           "an intent that closes nothing runs only the unconditional steps");
    expect(count_of(recorded(DeliveryFacts{.font_size_adjusted = true}),
                    DeliveryStep::announce_font_size) == 1,
           "changing the font size announces it once");
}

// 入力行の取り消しは今までどおり: 変換が消えたときだけ、確定と取消の意図では触らない。
void verify_command_composition()
{
    const auto cleared = recorded(DeliveryFacts{.command_composed = true});
    expect(count_of(cleared, DeliveryStep::cancel_command_composition) == 1 &&
               position_of(cleared, DeliveryStep::cancel_command_composition) <
                   position_of(cleared, DeliveryStep::follow_ime),
           "a command line that lost its composition cancels the IME's before follow_ime");
    expect(
        count_of(recorded(DeliveryFacts{.command_composed = true, .composition_settled = true}),
                 DeliveryStep::cancel_command_composition) == 0 &&
            count_of(recorded(DeliveryFacts{.command_composed = true, .command_composing = true}),
                     DeliveryStep::cancel_command_composition) == 0 &&
            count_of(recorded(DeliveryFacts{.command_composing = true}),
                     DeliveryStep::cancel_command_composition) == 0,
        "a committed, cancelled, kept or newly started composition is left to the IME");
}

void verify_closing_routes()
{
    expect(cancels_once(after(clicked(core::SettingsPart::close))),
           "releasing the close mark cancels the composition once before follow_ime");
    expect(cancels_once(after(clicked(core::SettingsPart::close_button))),
           "releasing the close button cancels the composition once before follow_ime");
    expect(cancels_once(after(key(VK_ESCAPE, false, false))),
           "Esc cancels the composition once before follow_ime");
    expect(cancels_once(after(key(VK_OEM_COMMA, true, false))),
           "Ctrl+, cancels the composition once before follow_ime");
    expect(cancels_once(after(clicked(core::SettingsPart::outside))),
           "a left press outside the panel cancels the composition once before follow_ime");
    expect(cancels_once(after(
               settings_right_press_intents(core::SettingsTarget{core::SettingsPart::outside, 0}))),
           "a right press outside the panel cancels the composition once before follow_ime");
}

void verify_open_routes()
{
    expect(leaves_ime(after(key(VK_OEM_COMMA, true, true))) &&
               leaves_ime(after(key(VK_ESCAPE, false, true))),
           "a held Ctrl+, or Esc repeating does not close and leaves the IME alone");
    const core::SettingsTarget row{core::SettingsPart::row, guide_row};
    const auto [pressed, silent] = settings_press_step(row);
    expect(leaves_ime(after(silent)) &&
               leaves_ime(after(settings_release_intents(pressed, row, sample()))) &&
               leaves_ime(after(clicked(core::SettingsPart::panel))),
           "pressing and releasing inside the panel leaves the IME alone");
    wchar_t pending = 0;
    expect(leaves_ime(after(settings_character_intents(L'a', false, pending, sample()))) &&
               leaves_ime(after(key(VK_BACK, false, false))),
           "typing in the search field leaves the IME alone");
    expect(
        leaves_ime(recorded(DeliveryFacts{.settings_open = true})) &&
            leaves_ime(recorded(DeliveryFacts{.settings_was_open = true, .settings_open = true})),
        "opening the panel and keeping it open do not cancel the composition");
}
} // namespace

void verify_delivery_steps()
{
    verify_order();
    verify_command_composition();
    verify_closing_routes();
    verify_open_routes();
}
} // namespace nenenib::tests::ui
