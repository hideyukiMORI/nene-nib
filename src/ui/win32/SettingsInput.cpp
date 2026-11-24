// 設定の面の鍵・文字・マウス → 面の意図の列（施主決定 D102・#1836 の差し戻し R1・R5）。
#include "SettingsInput.hpp"

#include "ChangeSetting.hpp"
#include "ChooseSettingsChoice.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "CloseSettingsChoice.hpp"
#include "FilterSettings.hpp"
#include "FocusSettingsRow.hpp"
#include "JumpSettingsSection.hpp"
#include "OpenSettingsChoice.hpp"
#include "PointSettingsChoice.hpp"
#include "RevertSettings.hpp"
#include "ScrollSettingsChoice.hpp"
#include "SettingsChoiceKeyAction.hpp"
#include "SettingsChoiceView.hpp"
#include "SettingsChoices.hpp"
#include "SettingsControl.hpp"
#include "SettingsKeyAction.hpp"
#include "SettingsKeys.hpp"
#include "SettingsPanel.hpp"
#include "SettingsPart.hpp"
#include "StepSettings.hpp"
#include "TypeSettingsChoice.hpp"
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

// 見えている行 row の右端の部品が選ぶ欄か（一覧を開く行・#1926）。
[[nodiscard]] bool chooses(const application::SettingsView &view, std::size_t row)
{
    return row < view.rows.size() &&
           core::settings_control_of(view.rows.at(row).row.subject, view.rows.at(row).switchable) ==
               core::SettingsControl::choice;
}

// 焦点の行が選ぶ欄か。
[[nodiscard]] bool focus_chooses(const application::SettingsView &view)
{
    return view.focused.has_value() && chooses(view, view.focused.value());
}

// Enter と（探す欄が空のときの）Space。選ぶ欄の行は一覧を開き、ほかの行は今までどおり切り替える
// （スイッチ・言語サーバーの行の「使う」「外す」・畳み・#1926 で Enter の意味が「次へ送る」から
// 「開く」に変わった）。
[[nodiscard]] SettingsIntents changed_or_opened(const application::SettingsView &view)
{
    if (focus_chooses(view))
    {
        return {application::OpenSettingsChoice{}};
    }
    return {application::ChangeSetting{true}};
}

// 一覧の選ぼうとしている位置を at へ（今と同じなら何もしない）。
[[nodiscard]] SettingsIntents pointed(const application::SettingsChoiceView &choice, std::size_t at)
{
    if (at == choice.candidate)
    {
        return {};
    }
    return {application::PointSettingsChoice{at}};
}

// 表示できる 1 文字 text（UTF-8）。一覧が開いている間の Space は決める、ほかの字は頭文字の跳び
// （探す欄には入れない・#1926）。閉じていれば、探す欄が空のときの Space
// は焦点の行の切り替えか一覧を 開く、ほかは探す欄へ。
[[nodiscard]] SettingsIntents printed(const std::string &text,
                                      const application::SettingsView &view)
{
    const bool space = text == " ";
    if (view.choice.has_value())
    {
        if (space)
        {
            return {application::ChooseSettingsChoice{}};
        }
        return {application::TypeSettingsChoice{view.choice.value().typed + text}};
    }
    if (space && view.query.empty())
    {
        return changed_or_opened(view);
    }
    return {application::FilterSettings{view.query + text}};
}

// ↑ ↓ の 1 歩（端で止まる・回らない）。
[[nodiscard]] SettingsIntents stepped_choice(const application::SettingsChoiceView &choice,
                                             bool down)
{
    const std::size_t at = choice.candidate;
    const bool stopped = down ? at + 1 >= choice.choices.size() : at == 0;
    if (stopped)
    {
        return {};
    }
    return pointed(choice, down ? at + 1 : at - 1);
}

// 長押しの反復では閉じない（Esc を押し続けても一覧の次に面まで閉じない）。
[[nodiscard]] SettingsIntents unless_repeated(const SettingsKeyPress &press,
                                              const application::EditorIntent &intent)
{
    if (press.repeated)
    {
        return {};
    }
    return {intent};
}

// Backspace は打った字の 1 つ（空なら何もしない）。
[[nodiscard]] SettingsIntents erased_typing(const application::SettingsChoiceView &choice)
{
    if (choice.typed.empty())
    {
        return {};
    }
    return {application::TypeSettingsChoice{without_last_character(choice.typed)}};
}

// 一覧が開いている間の鍵（#1926）。
[[nodiscard]] SettingsIntents choice_acted(SettingsChoiceKeyAction action,
                                           const SettingsKeyPress &press,
                                           const application::SettingsChoiceView &choice,
                                           std::size_t rows)
{
    const std::size_t count = choice.choices.size();
    switch (action)
    {
    case SettingsChoiceKeyAction::swallow:
        return {};
    case SettingsChoiceKeyAction::previous:
        return stepped_choice(choice, false);
    case SettingsChoiceKeyAction::next:
        return stepped_choice(choice, true);
    case SettingsChoiceKeyAction::first:
        return pointed(choice, 0);
    case SettingsChoiceKeyAction::last:
        return pointed(choice, std::max<std::size_t>(count, 1) - 1);
    case SettingsChoiceKeyAction::page_up:
        return pointed(choice, core::settings_choice_paged(count, choice.candidate, false, rows));
    case SettingsChoiceKeyAction::page_down:
        return pointed(choice, core::settings_choice_paged(count, choice.candidate, true, rows));
    case SettingsChoiceKeyAction::choose:
        return {application::ChooseSettingsChoice{}};
    case SettingsChoiceKeyAction::cancel:
        return unless_repeated(press, application::CloseSettingsChoice{});
    case SettingsChoiceKeyAction::close_panel:
        return unless_repeated(press, application::CloseSettings{});
    case SettingsChoiceKeyAction::erase:
        return erased_typing(choice);
    }
    std::unreachable();
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

// 見えている行 row が「ほかの N 言語を見る」の 1 行か（行のどこを押しても開く・#1945）。
[[nodiscard]] bool folds(const application::SettingsView &view, std::size_t row) noexcept
{
    return view.servers_hidden > 0 && row == view.rows.size() + view.servers.size();
}

// 部品 part が行の一部（index が見えている行の位置）か。
[[nodiscard]] bool on_row(core::SettingsPart part) noexcept
{
    switch (part)
    {
    case core::SettingsPart::outside:
    case core::SettingsPart::panel:
    case core::SettingsPart::search:
    case core::SettingsPart::close:
    case core::SettingsPart::close_button:
    case core::SettingsPart::chip:
    case core::SettingsPart::preset:
    case core::SettingsPart::revert:
        return false;
    case core::SettingsPart::row:
    case core::SettingsPart::row_value:
    case core::SettingsPart::row_minus:
    case core::SettingsPart::row_plus:
        return true;
    }
    std::unreachable();
}

// 部品 target が「言語サーバー」の節の行なら、その行の身元（ほかの部品と行では空）。
[[nodiscard]] std::string server_under(core::SettingsTarget target,
                                       const application::SettingsView &view)
{
    if (!on_row(target.part) || target.index < view.rows.size() ||
        target.index - view.rows.size() >= view.servers.size())
    {
        return {};
    }
    return view.servers.at(target.index - view.rows.size()).language;
}

// 行のクリックは焦点を置き、スイッチの行と畳みの行はどこを押しても切り替える。選ぶ欄は欄を押すと
// 一覧を開き（#1926）、段の上下は − と
// ＋、言語サーバーの行の「使う」「外す」はボタンを押したときだけ。
[[nodiscard]] SettingsIntents row_clicked(core::SettingsTarget target,
                                          const application::SettingsView &view)
{
    SettingsIntents intents{application::FocusSettingsRow{target.index}};
    // 選ぶ欄の値の箱は一覧を開く（#1926）。
    if (target.part == core::SettingsPart::row_value && chooses(view, target.index))
    {
        intents.emplace_back(application::OpenSettingsChoice{});
        return intents;
    }
    if (target.part != core::SettingsPart::row || toggles(view, target.index) ||
        folds(view, target.index))
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
        return changed_or_opened(view);
    case SettingsKeyAction::open_choice:
        return focus_chooses(view) ? SettingsIntents{application::OpenSettingsChoice{}}
                                   : SettingsIntents{};
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
                                     const std::optional<core::SettingsLayout> &layout,
                                     std::size_t choice_rows)
{
    // 一覧が開いている間の鍵は一覧へ（面の鍵・節の鍵へ通さない・#1926）。
    if (view.choice.has_value())
    {
        return choice_acted(settings_choice_key_action(press.key, press.control), press,
                            view.choice.value(), choice_rows);
    }
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
    std::wstring wide;
    if (low)
    {
        wide.push_back(high);
    }
    wide.push_back(unit);
    return printed(core::to_utf8(wide).value_or(std::string{}), view);
}

std::optional<SettingsIntents> settings_system_key_intents(WPARAM key,
                                                           const application::SettingsView &view)
{
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (key)
    {
    case VK_DOWN:
        return !view.choice.has_value() && focus_chooses(view)
                   ? SettingsIntents{application::OpenSettingsChoice{}}
                   : SettingsIntents{};
    case VK_UP:
        return view.choice.has_value() ? SettingsIntents{application::CloseSettingsChoice{}}
                                       : SettingsIntents{};
    default:
        return std::nullopt;
    }
}

std::pair<std::optional<std::size_t>, SettingsIntents>
settings_choice_press_step(bool inside, std::optional<std::size_t> choice)
{
    if (!inside)
    {
        return {std::nullopt, SettingsIntents{application::CloseSettingsChoice{}}};
    }
    if (choice.has_value())
    {
        return {choice, SettingsIntents{application::PointSettingsChoice{choice.value()}}};
    }
    return {std::nullopt, SettingsIntents{}};
}

SettingsIntents settings_choice_release_intents(std::optional<std::size_t> pressed,
                                                std::optional<std::size_t> released)
{
    if (!pressed.has_value() || pressed != released)
    {
        return {};
    }
    return {application::PointSettingsChoice{pressed.value()}, application::ChooseSettingsChoice{}};
}

std::pair<std::optional<std::size_t>, SettingsIntents>
settings_choice_hover_step(std::optional<std::size_t> hovered, std::optional<std::size_t> under)
{
    if (!under.has_value() || under == hovered)
    {
        return {hovered, SettingsIntents{}};
    }
    return {under, SettingsIntents{application::PointSettingsChoice{under.value()}}};
}

std::optional<SettingsIntents> settings_choice_right_press_intents(bool open)
{
    if (!open)
    {
        return std::nullopt;
    }
    return SettingsIntents{application::CloseSettingsChoice{}};
}

std::optional<SettingsIntents> settings_choice_wheel_intents(bool inside, bool over_panel,
                                                             std::optional<std::size_t> scrolled)
{
    if (inside)
    {
        return scrolled.has_value()
                   ? SettingsIntents{application::ScrollSettingsChoice{scrolled.value()}}
                   : SettingsIntents{};
    }
    if (over_panel)
    {
        return SettingsIntents{};
    }
    return std::nullopt;
}

SettingsIntents settings_choice_resize_intents(bool open)
{
    if (!open)
    {
        return {};
    }
    return {application::CloseSettingsChoice{}};
}

std::pair<std::optional<SettingsPress>, SettingsIntents>
settings_press_step(core::SettingsTarget target, const application::SettingsView &view)
{
    if (target.part == core::SettingsPart::outside)
    {
        return {std::nullopt, SettingsIntents{application::CloseSettings{}}};
    }
    return {SettingsPress{target, server_under(target, view)}, SettingsIntents{}};
}

SettingsIntents settings_release_intents(const std::optional<SettingsPress> &pressed,
                                         core::SettingsTarget released,
                                         const application::SettingsView &view)
{
    // 押した部品と離した部品が同じで、言語サーバーの行なら同じ身元の行のときだけ動かす（ADR 0056 の
    // 決定 9 と同じ考え・押している間に行がずれたら捨てる・#1945 のレビュー R2）。
    if (!pressed.has_value() || !(pressed.value().target == released) ||
        pressed.value().server != server_under(released, view))
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
