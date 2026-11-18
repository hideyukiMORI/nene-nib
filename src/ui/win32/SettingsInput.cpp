// 設定の面の鍵・文字・マウス → 面の意図の列（施主決定 D102・#1836 の差し戻し R1・R5）。
#include "SettingsInput.hpp"

#include "ChangeSetting.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "FilterSettings.hpp"
#include "FocusSettingsRow.hpp"
#include "JumpSettingsSection.hpp"
#include "RevertSettings.hpp"
#include "SettingsControl.hpp"
#include "SettingsKeyAction.hpp"
#include "SettingsKeys.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPart.hpp"
#include "StepSettings.hpp"
#include "Utf16.hpp"

#include <cstddef>
#include <limits>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
constexpr wchar_t first_high_surrogate = 0xD800;
constexpr wchar_t first_low_surrogate = 0xDC00;
constexpr wchar_t last_low_surrogate = 0xDFFF;
constexpr wchar_t first_printable = 0x20;
constexpr wchar_t delete_character = 0x7F;
constexpr unsigned char continuation_mask = 0xC0;
constexpr unsigned char continuation_mark = 0x80;

// 最後の 1 文字（UTF-8 のコードポイント）を除いた文字。
[[nodiscard]] std::string without_last_character(std::string text)
{
    while (!text.empty() &&
           (static_cast<unsigned char>(text.back()) & continuation_mask) == continuation_mark)
    {
        text.pop_back();
    }
    if (!text.empty())
    {
        text.pop_back();
    }
    return text;
}

// 見えている行 row の右端の部品がスイッチか。
[[nodiscard]] bool toggles(const application::SettingsView &view, std::size_t row)
{
    return row < view.rows.size() &&
           core::settings_control_of(view.rows.at(row).row.subject, view.rows.at(row).switchable) ==
               core::SettingsControl::toggle;
}

// ← →。値の行は前後へ送る。スイッチの行は ← でオフ・→ でオン（もうその側なら何もしない）。
[[nodiscard]] SettingsIntents turned(const application::SettingsView &view, bool forward)
{
    if (!view.focused.has_value() || view.focused.value() >= view.rows.size())
    {
        return {};
    }
    const std::size_t row = view.focused.value();
    if (toggles(view, row) && view.rows.at(row).on == forward)
    {
        return {};
    }
    return {application::ChangeSetting{forward}};
}

// PageUp / PageDown。行き先は最後に描いた配置から引く（描く前は 1 行だけ動く）。
[[nodiscard]] SettingsIntents paged(const application::SettingsView &view,
                                    const std::optional<core::SettingsLayout> &layout, bool down)
{
    const auto row = layout.has_value()
                         ? core::settings_paged_row(layout.value(), view.focused, down)
                         : std::nullopt;
    if (row.has_value())
    {
        return {application::FocusSettingsRow{row.value()}};
    }
    return {application::StepSettings{down}};
}

// 行のクリックは焦点を置き、スイッチの行はどこを押しても切り替える。選ぶ欄は欄を押すと次の値、
// 段の上下は − と ＋。
[[nodiscard]] SettingsIntents row_clicked(core::SettingsTarget target,
                                          const application::SettingsView &view)
{
    SettingsIntents intents{application::FocusSettingsRow{target.index}};
    if (target.part != core::SettingsPart::row || toggles(view, target.index))
    {
        intents.emplace_back(
            application::ChangeSetting{target.part != core::SettingsPart::row_minus});
    }
    return intents;
}

[[nodiscard]] SettingsIntents clicked(core::SettingsTarget target,
                                      const application::SettingsView &view)
{
    switch (target.part)
    {
    case core::SettingsPart::outside:
    case core::SettingsPart::panel:
    case core::SettingsPart::search:
        return {};
    case core::SettingsPart::close:
    case core::SettingsPart::close_button:
        return {application::CloseSettings{}};
    case core::SettingsPart::chip:
        return {application::JumpSettingsSection{core::settings_sections.at(target.index)}};
    case core::SettingsPart::preset:
        return {application::ChooseSettingsPreset{core::settings_presets.at(target.index)}};
    case core::SettingsPart::revert:
        return {application::RevertSettings{}};
    case core::SettingsPart::row:
    case core::SettingsPart::row_value:
    case core::SettingsPart::row_minus:
    case core::SettingsPart::row_plus:
        return row_clicked(target, view);
    }
    std::unreachable();
}

// Backspace は探す欄の最後の 1 文字（空なら何もしない）。
[[nodiscard]] SettingsIntents erased(const application::SettingsView &view)
{
    if (view.query.empty())
    {
        return {};
    }
    return {application::FilterSettings{without_last_character(view.query)}};
}

// Esc と Ctrl+,。長押しの反復では閉じない（Ctrl+, を押し続けても開閉を繰り返さない）。
[[nodiscard]] SettingsIntents closed(bool repeated)
{
    if (repeated)
    {
        return {};
    }
    return {application::CloseSettings{}};
}

[[nodiscard]] SettingsIntents acted(SettingsKeyAction action, const SettingsKeyPress &press,
                                    const application::SettingsView &view,
                                    const std::optional<core::SettingsLayout> &layout)
{
    switch (action)
    {
    case SettingsKeyAction::swallow:
        return {};
    case SettingsKeyAction::close:
        return closed(press.repeated);
    case SettingsKeyAction::revert:
        return {application::RevertSettings{}};
    case SettingsKeyAction::step_up:
        return {application::StepSettings{false}};
    case SettingsKeyAction::step_down:
        return {application::StepSettings{true}};
    case SettingsKeyAction::page_up:
        return paged(view, layout, false);
    case SettingsKeyAction::page_down:
        return paged(view, layout, true);
    case SettingsKeyAction::first:
        return {application::FocusSettingsRow{0}};
    case SettingsKeyAction::last:
        return {application::FocusSettingsRow{std::numeric_limits<std::size_t>::max()}};
    case SettingsKeyAction::change:
        return {application::ChangeSetting{true}};
    case SettingsKeyAction::previous_value:
        return turned(view, false);
    case SettingsKeyAction::next_value:
        return turned(view, true);
    case SettingsKeyAction::erase:
        return erased(view);
    }
    std::unreachable();
}
} // namespace

SettingsIntents settings_key_intents(const SettingsKeyPress &press,
                                     const application::SettingsView &view,
                                     const std::optional<core::SettingsLayout> &layout)
{
    const auto section = settings_section_key(press.key, press.control);
    if (section.has_value())
    {
        return {application::JumpSettingsSection{core::settings_sections.at(section.value())}};
    }
    return acted(settings_key_action(press.key, press.control, press.shift), press, view, layout);
}

SettingsIntents settings_character_intents(wchar_t unit, bool control, wchar_t &pending,
                                           const application::SettingsView &view)
{
    // Ctrl つきの文字は受けない（上位サロゲートの覚えも捨てる）。
    if (control)
    {
        pending = 0;
        return {};
    }
    if (unit >= first_high_surrogate && unit < first_low_surrogate)
    {
        pending = unit;
        return {};
    }
    const wchar_t high = std::exchange(pending, wchar_t{0});
    const bool low = unit >= first_low_surrogate && unit <= last_low_surrogate;
    // 制御文字（Enter・Tab・Backspace）は WM_KEYDOWN が扱う。
    if (unit < first_printable || unit == delete_character || (low && high == 0))
    {
        return {};
    }
    if (unit == first_printable && view.query.empty())
    {
        return {application::ChangeSetting{true}};
    }
    std::wstring wide;
    if (low)
    {
        wide.push_back(high);
    }
    wide.push_back(unit);
    return {application::FilterSettings{view.query + core::to_utf8(wide).value_or(std::string{})}};
}

std::pair<std::optional<core::SettingsTarget>, SettingsIntents>
settings_press_step(core::SettingsTarget target)
{
    if (target.part == core::SettingsPart::outside)
    {
        return {std::nullopt, SettingsIntents{application::CloseSettings{}}};
    }
    return {target, SettingsIntents{}};
}

SettingsIntents settings_release_intents(std::optional<core::SettingsTarget> pressed,
                                         core::SettingsTarget released,
                                         const application::SettingsView &view)
{
    // 押した部品と離した部品が同じときだけ動かす（ADR 0056 の決定 9 と同じ考え）。
    if (!pressed.has_value() || !(pressed.value() == released))
    {
        return {};
    }
    return clicked(released, view);
}

SettingsIntents settings_right_press_intents(core::SettingsTarget target)
{
    if (target.part == core::SettingsPart::outside)
    {
        return {application::CloseSettings{}};
    }
    return {};
}
} // namespace nenenib::ui::win32
