// #1836 の差し戻し（レビュー M1・R1・R5）: 設定の面が開いている間の鍵・文字・マウスの振り分け。窓は
// 作らず、窓の手続きが値を集めて呼ぶ純関数（SettingsInput）に鍵・文字・押しを流し、返った意図の列を
// 確かめる。面の間はどの鍵も面の意図しか返さない（本文を変える意図が 1 つも無い）・Esc と Ctrl+, は
// CloseSettings の 1 つ（保存の 1 回は application の `--settings-modal` が見る）・長押しの反復では
// 閉じない・Ctrl+Space を受けない・面の外の押しは閉じるだけ・面の外のホイールは面が取らない・
// 面が閉じた所で変換の取り消しを 1 回だけ呼ぶ。IME の実機は試験にできないので、取り消しを呼ぶ
// 1 か所（settings_just_closed）を通る回数を数える。
#include "ChangeSetting.hpp"
#include "ChooseSettingsChoice.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "CloseSettingsChoice.hpp"
#include "EditorFeature.hpp"
#include "EditorIntent.hpp"
#include "FilterSettings.hpp"
#include "FocusSettingsRow.hpp"
#include "JumpSettingsSection.hpp"
#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"
#include "LanguageServerRowView.hpp"
#include "OpenSettingsChoice.hpp"
#include "PointSettingsChoice.hpp"
#include "RevertSettings.hpp"
#include "ScrollSettings.hpp"
#include "SettingsChoiceView.hpp"
#include "SettingsInput.hpp"
#include "SettingsKeyPress.hpp"
#include "SettingsLayout.hpp"
#include "SettingsPart.hpp"
#include "SettingsPreset.hpp"
#include "SettingsPress.hpp"
#include "SettingsRow.hpp"
#include "SettingsRowView.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsSubject.hpp"
#include "SettingsTarget.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "StepSettings.hpp"
#include "TypeSettingsChoice.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using nenenib::ui::win32::settings_character_intents;
using nenenib::ui::win32::settings_just_closed;
using nenenib::ui::win32::settings_key_intents;
using nenenib::ui::win32::settings_press_step;
using nenenib::ui::win32::settings_release_intents;
using nenenib::ui::win32::settings_right_press_intents;
using nenenib::ui::win32::settings_takes_wheel;
using nenenib::ui::win32::SettingsIntents;
using nenenib::ui::win32::SettingsKeyPress;
using nenenib::ui::win32::SettingsPress;

constexpr std::size_t guide_row = 1;
constexpr std::size_t size_row = 2;
constexpr WPARAM key_count = 256;

// 面をまだ描いていない（配置が無い）。
[[nodiscard]] const std::optional<core::SettingsLayout> &undrawn()
{
    static const std::optional<core::SettingsLayout> none;
    return none;
}

[[nodiscard]] app::SettingsRowView row_of(core::SettingsSection section,
                                          const core::SettingsSubject &subject, bool on)
{
    return app::SettingsRowView{core::SettingsRow{section, subject},
                                core::settings_row_title(subject),
                                core::settings_row_description(subject),
                                core::settings_row_set_name(subject),
                                on,
                                true,
                                {},
                                std::nullopt,
                                false};
}

// 見本の面: 配色（選ぶ欄）・操作の案内（オフのスイッチ）・字の大きさ（段の上下）。
[[nodiscard]] app::SettingsView sample(std::optional<std::size_t> focused, std::string query)
{
    app::SettingsView view{};
    view.rows.push_back(row_of(core::SettingsSection::display, core::SettingsValue::theme, false));
    view.rows.push_back(row_of(core::SettingsSection::display, core::EditorFeature::guide, false));
    view.rows.push_back(row_of(core::SettingsSection::text, core::SettingsValue::font_size, false));
    view.focused = focused;
    view.query = std::move(query);
    return view;
}

// 面の意図か（面を開いたまま受ける・本文も選びも変えない）。
[[nodiscard]] bool is_panel_intent(const app::EditorIntent &intent) noexcept
{
    return std::holds_alternative<app::CloseSettings>(intent) ||
           std::holds_alternative<app::RevertSettings>(intent) ||
           std::holds_alternative<app::StepSettings>(intent) ||
           std::holds_alternative<app::ChangeSetting>(intent) ||
           std::holds_alternative<app::ChooseSettingsPreset>(intent) ||
           std::holds_alternative<app::FilterSettings>(intent) ||
           std::holds_alternative<app::JumpSettingsSection>(intent) ||
           std::holds_alternative<app::FocusSettingsRow>(intent) ||
           std::holds_alternative<app::ScrollSettings>(intent) ||
           std::holds_alternative<app::OpenSettingsChoice>(intent) ||
           std::holds_alternative<app::PointSettingsChoice>(intent) ||
           std::holds_alternative<app::ChooseSettingsChoice>(intent) ||
           std::holds_alternative<app::CloseSettingsChoice>(intent) ||
           std::holds_alternative<app::TypeSettingsChoice>(intent);
}

// 配色の行（焦点）に一覧を開いた見本（12 個の選択肢・今の値 2・選ぼうとしている位置 at）。
[[nodiscard]] app::SettingsView listed(std::size_t at, std::string typed)
{
    auto view = sample(0, {});
    std::vector<std::string> choices;
    for (std::size_t index = 0; index < 12; ++index)
    {
        choices.push_back("choice-" + std::to_string(index));
    }
    view.choice = app::SettingsChoiceView{std::move(choices), 2, at, 0, std::move(typed)};
    return view;
}

[[nodiscard]] bool points_at(const SettingsIntents &intents, std::size_t at)
{
    return intents.size() == 1 &&
           std::holds_alternative<app::PointSettingsChoice>(intents.front()) &&
           std::get<app::PointSettingsChoice>(intents.front()).choice == at;
}

// 言語サーバーの行の無い所の押しの覚え（身元は空）。
[[nodiscard]] std::optional<SettingsPress> pressing(core::SettingsTarget target)
{
    return SettingsPress{target, {}};
}

[[nodiscard]] app::LanguageServerRowView server_row(std::string language,
                                                    app::LanguageServerRowAction action)
{
    return app::LanguageServerRowView{
        language, language, app::LanguageServerRowState::configured, {}, true, action, language};
}

template <typename Intent> [[nodiscard]] bool only(const SettingsIntents &intents)
{
    return intents.size() == 1 && std::holds_alternative<Intent>(intents.front());
}

[[nodiscard]] SettingsIntents pressed(WPARAM key, bool control, bool repeated,
                                      const app::SettingsView &view)
{
    return settings_key_intents(SettingsKeyPress{key, control, false, repeated}, view, undrawn());
}

[[nodiscard]] SettingsIntents typed(wchar_t unit, bool control, const app::SettingsView &view)
{
    wchar_t pending = 0;
    return settings_character_intents(unit, control, pending, view);
}

// 鍵 key の 4 通りの押しと、同じ番号の文字が、面の意図のほかを返さないか。閉じた回数を closes
// に足す。
[[nodiscard]] bool contained(WPARAM key, const app::SettingsView &view, std::size_t &closes)
{
    bool panel_only = true;
    for (const auto &press :
         {SettingsKeyPress{key, false, false, false}, SettingsKeyPress{key, true, false, false},
          SettingsKeyPress{key, false, true, false}, SettingsKeyPress{key, true, true, true}})
    {
        const auto intents = settings_key_intents(press, view, undrawn());
        panel_only =
            panel_only && intents.size() <= 1 && std::ranges::all_of(intents, is_panel_intent);
        closes += only<app::CloseSettings>(intents) ? 1 : 0;
    }
    const auto unit = static_cast<wchar_t>(key);
    return panel_only && std::ranges::all_of(typed(unit, false, view), is_panel_intent) &&
           typed(unit, true, view).empty();
}

// どの鍵・どの文字も、面の意図のほかは返さない（開いている間に本文が変わらない）。
void verify_nothing_reaches_the_text()
{
    bool panel_only = true;
    std::size_t closes = 0;
    for (const auto &view : {sample(guide_row, {}), sample(std::nullopt, "wr")})
    {
        for (WPARAM key = 0; key < key_count; ++key)
        {
            panel_only = contained(key, view, closes) && panel_only;
        }
    }
    expect(panel_only, "every key and character of the open panel maps to panel intents only");
    // 閉じるのは Esc（Shift つきも）と Ctrl+,（反復でない押し）だけ。2 つの view で 3 回ずつ。
    expect(closes == 6, "only Esc and Ctrl+, close the panel");
}

void verify_closing_keys()
{
    const auto view = sample(guide_row, {});
    expect(only<app::CloseSettings>(pressed(VK_ESCAPE, false, false, view)) &&
               only<app::CloseSettings>(pressed(VK_OEM_COMMA, true, false, view)),
           "Esc and Ctrl+, send one CloseSettings (one save)");
    expect(pressed(VK_OEM_COMMA, true, true, view).empty() &&
               pressed(VK_ESCAPE, false, true, view).empty(),
           "a held Ctrl+, does not close and reopen on auto-repeat");
    expect(pressed(VK_F1, false, false, view).empty() && pressed('S', true, false, view).empty() &&
               pressed(VK_TAB, true, false, view).empty() &&
               pressed('J', false, false, view).empty(),
           "F1, Ctrl+S, Ctrl+Tab and letter keys are swallowed");
    expect(only<app::RevertSettings>(pressed('Z', true, false, view)) &&
               only<app::JumpSettingsSection>(pressed('6', true, false, view)) &&
               only<app::StepSettings>(pressed(VK_NEXT, false, false, view)),
           "Ctrl+Z reverts, Ctrl+6 jumps and paging steps a row before the first paint");
}

void verify_values_and_search()
{
    const auto off = sample(guide_row, {});
    expect(pressed(VK_LEFT, false, false, off).empty() &&
               only<app::ChangeSetting>(pressed(VK_RIGHT, false, false, off)) &&
               only<app::ChangeSetting>(pressed(VK_LEFT, false, false, sample(size_row, {}))) &&
               pressed(VK_RIGHT, false, false, sample(std::nullopt, {})).empty(),
           "the arrows turn a switch only toward the other side and step a value both ways");
    const auto erased = pressed(VK_BACK, false, false, sample(guide_row, "折り"));
    expect(only<app::FilterSettings>(erased) &&
               std::get<app::FilterSettings>(erased.front()).query == "折" &&
               pressed(VK_BACK, false, false, off).empty(),
           "Backspace removes one character of the search and nothing when it is empty");
}

void verify_characters()
{
    const auto empty = sample(guide_row, {});
    const auto filled = sample(guide_row, "wr");
    // Ctrl+Space は WM_CHAR 0x20 を出す（この機械の配列の実測）。面は受けない（R1）。
    expect(typed(L' ', true, empty).empty() && typed(L' ', true, filled).empty() &&
               typed(L'a', true, empty).empty(),
           "Ctrl+Space and other Ctrl characters neither toggle nor reach the search");
    const auto spaced = typed(L' ', false, filled);
    expect(only<app::ChangeSetting>(typed(L' ', false, empty)) &&
               only<app::FilterSettings>(spaced) &&
               std::get<app::FilterSettings>(spaced.front()).query == "wr ",
           "Space toggles while the search is empty and is a blank after typing");
    const auto lettered = typed(L'a', false, filled);
    expect(only<app::FilterSettings>(lettered) &&
               std::get<app::FilterSettings>(lettered.front()).query == "wra" &&
               typed(L'\r', false, filled).empty() && typed(L'\t', false, filled).empty() &&
               typed(L'\x1B', false, filled).empty() && typed(L'\x7F', false, filled).empty(),
           "letters go to the search and control characters are left to the key table");
    wchar_t pending = 0;
    const auto high = settings_character_intents(L'\xD83D', false, pending, filled);
    const auto pair = settings_character_intents(L'\xDE00', false, pending, filled);
    expect(high.empty() && only<app::FilterSettings>(pair) &&
               std::get<app::FilterSettings>(pair.front()).query.size() == 6 && pending == 0 &&
               typed(L'\xDE00', false, filled).empty(),
           "a surrogate pair is one character and a lone low surrogate is dropped");
}

void verify_mouse()
{
    using core::SettingsPart;
    using core::SettingsTarget;
    const auto view = sample(std::nullopt, {});
    const auto [nothing, closing] =
        settings_press_step(SettingsTarget{SettingsPart::outside, 0}, view);
    expect(!nothing.has_value() && only<app::CloseSettings>(closing) &&
               settings_release_intents(nothing, SettingsTarget{SettingsPart::outside, 0}, view)
                   .empty(),
           "a press outside only closes the panel; nothing is held for the release");
    const SettingsTarget guide{SettingsPart::row, guide_row};
    const auto [held, silent] = settings_press_step(guide, view);
    const auto clicked = settings_release_intents(held, guide, view);
    expect(held.has_value() && held.value().target == guide && held.value().server.empty() &&
               silent.empty() && clicked.size() == 2 &&
               std::holds_alternative<app::FocusSettingsRow>(clicked.front()) &&
               std::holds_alternative<app::ChangeSetting>(clicked.back()),
           "a switch row toggles when it is released where it was pressed");
    expect(
        settings_release_intents(held, SettingsTarget{SettingsPart::row, size_row}, view).empty() &&
            settings_release_intents(held, SettingsTarget{SettingsPart::outside, 0}, view)
                .empty() &&
            only<app::FocusSettingsRow>(
                settings_release_intents(pressing(SettingsTarget{SettingsPart::row, size_row}),
                                         SettingsTarget{SettingsPart::row, size_row}, view)),
        "releasing on another part does nothing and a value row only takes the focus");
    const SettingsTarget minus{SettingsPart::row_minus, size_row};
    const auto stepped = settings_release_intents(pressing(minus), minus, view);
    expect(stepped.size() == 2 && std::holds_alternative<app::ChangeSetting>(stepped.back()) &&
               !std::get<app::ChangeSetting>(stepped.back()).forward &&
               only<app::CloseSettings>(
                   settings_release_intents(pressing(SettingsTarget{SettingsPart::close, 0}),
                                            SettingsTarget{SettingsPart::close, 0}, view)) &&
               only<app::ChooseSettingsPreset>(
                   settings_release_intents(pressing(SettingsTarget{SettingsPart::preset, 2}),
                                            SettingsTarget{SettingsPart::preset, 2}, view)),
           "the minus button steps back, the close mark closes and a preset is chosen");
    expect(only<app::CloseSettings>(
               settings_right_press_intents(SettingsTarget{SettingsPart::outside, 0})) &&
               settings_right_press_intents(guide).empty(),
           "a right press outside closes and inside does nothing");
    expect(!settings_takes_wheel(SettingsTarget{SettingsPart::outside, 0}) &&
               settings_takes_wheel(SettingsTarget{SettingsPart::panel, 0}) &&
               settings_takes_wheel(guide),
           "the wheel over the text is left to the text and does not reach the panel");
}

// 押してから離すまでに探しの結果が届き、畳まれていた行が現れて後ろの行の位置がずれても、離した
// 位置に来た別の行の「使う」/「外す」は動かない（行の身元で比べる・#1945 のレビュー R2）。
void verify_press_survives_shifted_rows()
{
    using core::SettingsPart;
    using core::SettingsTarget;
    using Action = app::LanguageServerRowAction;
    auto before = sample(std::nullopt, {});
    before.servers = {server_row("cpp", Action::remove), server_row("zig", Action::remove)};
    // 「zig」の「外す」を押す。押している間に「python」の行（使う）が現れて zig の位置に来る。
    const SettingsTarget remove{SettingsPart::row_value, before.rows.size() + 1};
    const auto [held, silent] = settings_press_step(remove, before);
    auto after = before;
    after.servers = {server_row("cpp", Action::remove), server_row("python", Action::use),
                     server_row("zig", Action::remove)};
    expect(held.has_value() && held.value().server == "zig" && silent.empty() &&
               settings_release_intents(held, remove, after).empty(),
           "a row that slid under the pointer while the button was held does not act");
    const auto same = settings_release_intents(held, remove, before);
    expect(same.size() == 2 && std::holds_alternative<app::FocusSettingsRow>(same.front()) &&
               std::holds_alternative<app::ChangeSetting>(same.back()),
           "releasing on the same language row acts on it");
    const SettingsTarget moved{SettingsPart::row_value, after.rows.size() + 2};
    expect(settings_release_intents(held, moved, after).empty(),
           "the same language at another position is another part and does not act either");
}

// 面が閉じた所（開いていた → 閉じた）でだけ変換の取り消しを呼ぶ。閉じる道（Esc・Ctrl+,・✕・
// 「閉じる」・外の押し・application が閉じた意図）はどれも delivery の後のこの 1 か所を通る。
void verify_composition_is_cancelled_on_close()
{
    constexpr std::array<bool, 9> shown{false, true, true, false, false, true, false, true, true};
    std::size_t cancels = 0;
    bool was_open = false;
    for (const bool open : shown)
    {
        cancels += settings_just_closed(was_open, open) ? 1 : 0;
        was_open = open;
    }
    expect(cancels == 2, "the composition is cancelled once each time the panel closes");
    expect(!settings_just_closed(false, false) && !settings_just_closed(false, true) &&
               !settings_just_closed(true, true),
           "opening, staying open and staying closed do not touch the IME");
}

// #1926: 一覧を開く 4 つの道（値の箱のクリック・Enter / Space・Alt+↓・F4）と、開かずに送る ← →。
// スイッチの行の Enter / Space は今までどおり切り替える。
void verify_choice_opening()
{
    const auto theme = sample(0, {});
    const auto guide = sample(guide_row, {});
    expect(only<app::OpenSettingsChoice>(pressed(VK_RETURN, false, false, theme)) &&
               only<app::OpenSettingsChoice>(pressed(VK_F4, false, false, theme)) &&
               only<app::OpenSettingsChoice>(typed(L' ', false, theme)),
           "Enter, F4 and Space open the list of a choice row");
    expect(only<app::ChangeSetting>(pressed(VK_RETURN, false, false, guide)) &&
               only<app::ChangeSetting>(typed(L' ', false, guide)) &&
               pressed(VK_F4, false, false, guide).empty() &&
               only<app::ChangeSetting>(pressed(VK_RIGHT, false, false, theme)) &&
               only<app::ChangeSetting>(pressed(VK_LEFT, false, false, theme)),
           "a switch row still toggles and the arrows step a choice without opening it");
    using nenenib::ui::win32::settings_system_key_intents;
    const auto down = settings_system_key_intents(VK_DOWN, theme);
    const auto off = settings_system_key_intents(VK_DOWN, guide);
    const auto up = settings_system_key_intents(VK_UP, listed(2, {}));
    expect(down.has_value() && only<app::OpenSettingsChoice>(down.value()) && off.has_value() &&
               off.value().empty() && up.has_value() &&
               only<app::CloseSettingsChoice>(up.value()) &&
               !settings_system_key_intents(VK_F4, theme).has_value() &&
               !settings_system_key_intents('X', theme).has_value(),
           "Alt+Down opens, Alt+Up closes and other Alt keys are left to the system (Alt+F4)");
    using core::SettingsPart;
    using core::SettingsTarget;
    const SettingsTarget box{SettingsPart::row_value, 0};
    const auto clicked = settings_release_intents(pressing(box), box, theme);
    expect(clicked.size() == 2 && std::holds_alternative<app::FocusSettingsRow>(clicked.front()) &&
               std::holds_alternative<app::OpenSettingsChoice>(clicked.back()),
           "clicking the value box focuses the row and opens its list");
}

// #1926: 一覧が開いている間の鍵と字。↑ ↓ は端で止まる・Home / End・PgUp / PgDn・Enter / Space / Tab
// で決める・Esc と F4 は決めずに閉じる・Ctrl+, は面ごと閉じる・字は頭文字の跳び・Backspace は打った
// 字の 1 つ。ほかの鍵（← → と Ctrl+Z と節の鍵）は捨てる。
void verify_choice_keys()
{
    expect(points_at(pressed(VK_UP, false, false, listed(2, {})), 1) &&
               points_at(pressed(VK_DOWN, false, false, listed(2, {})), 3) &&
               pressed(VK_UP, false, false, listed(0, {})).empty() &&
               pressed(VK_DOWN, false, false, listed(11, {})).empty(),
           "the arrows move the chosen row one step and stop at the ends");
    expect(points_at(pressed(VK_HOME, false, false, listed(5, {})), 0) &&
               points_at(pressed(VK_END, false, false, listed(5, {})), 11) &&
               points_at(pressed(VK_NEXT, false, false, listed(1, {})), 10) &&
               points_at(pressed(VK_PRIOR, false, false, listed(5, {})), 0) &&
               pressed(VK_HOME, false, false, listed(0, {})).empty(),
           "Home, End and the page keys move to the ends and by a page");
    const auto list = listed(4, {});
    expect(only<app::ChooseSettingsChoice>(pressed(VK_RETURN, false, false, list)) &&
               only<app::ChooseSettingsChoice>(pressed(VK_TAB, false, false, list)) &&
               only<app::ChooseSettingsChoice>(typed(L' ', false, list)),
           "Enter, Tab and Space choose the row");
    expect(only<app::CloseSettingsChoice>(pressed(VK_ESCAPE, false, false, list)) &&
               only<app::CloseSettingsChoice>(pressed(VK_F4, false, false, list)) &&
               pressed(VK_ESCAPE, false, true, list).empty() &&
               only<app::CloseSettings>(pressed(VK_OEM_COMMA, true, false, list)),
           "Esc and F4 close the list first and Ctrl+, closes the whole panel");
    expect(pressed(VK_LEFT, false, false, list).empty() &&
               pressed(VK_RIGHT, false, false, list).empty() &&
               pressed('Z', true, false, list).empty() && pressed('1', true, false, list).empty(),
           "other keys are swallowed while the list is open");
    const auto letter = typed(L'S', false, listed(4, "o"));
    const auto erased = pressed(VK_BACK, false, false, listed(4, "ab"));
    expect(only<app::TypeSettingsChoice>(letter) &&
               std::get<app::TypeSettingsChoice>(letter.front()).typed == "oS" &&
               only<app::TypeSettingsChoice>(erased) &&
               std::get<app::TypeSettingsChoice>(erased.front()).typed == "a" &&
               pressed(VK_BACK, false, false, list).empty() && typed(L'a', true, list).empty(),
           "letters go to the jump, not to the search, and Backspace removes one of them");
    bool panel_only = true;
    for (WPARAM key = 0; key < key_count; ++key)
    {
        for (const bool control : {false, true})
        {
            panel_only = panel_only &&
                         std::ranges::all_of(pressed(key, control, false, list), is_panel_intent) &&
                         std::ranges::all_of(typed(static_cast<wchar_t>(key), control, list),
                                             is_panel_intent);
        }
    }
    expect(panel_only, "every key and character of the open list maps to panel intents only");
}

// #1926: 一覧の上のマウス。外の押しは閉じるだけ・行の押しはその行を選ぼうとしている行にする・同じ行
// で離すと決める・ほかの行や外で離すと何もしない・上下の空きは何もしない。
void verify_choice_mouse()
{
    using nenenib::ui::win32::settings_choice_press_step;
    using nenenib::ui::win32::settings_choice_release_intents;
    const auto [nothing, closing] = settings_choice_press_step(false, std::nullopt);
    const auto [held, pointing] = settings_choice_press_step(true, 3);
    const auto [gap, silent] = settings_choice_press_step(true, std::nullopt);
    expect(!nothing.has_value() && only<app::CloseSettingsChoice>(closing) &&
               held == std::optional<std::size_t>{3} && points_at(pointing, 3) &&
               !gap.has_value() && silent.empty(),
           "a press outside closes the list and a press on a row points at it");
    const auto chosen = settings_choice_release_intents(held, 3);
    expect(chosen.size() == 2 && std::holds_alternative<app::PointSettingsChoice>(chosen.front()) &&
               std::holds_alternative<app::ChooseSettingsChoice>(chosen.back()) &&
               settings_choice_release_intents(held, 4).empty() &&
               settings_choice_release_intents(held, std::nullopt).empty() &&
               settings_choice_release_intents(std::nullopt, 3).empty(),
           "releasing on the pressed row chooses it and anywhere else does nothing");
}
} // namespace

void verify_settings_input()
{
    verify_nothing_reaches_the_text();
    verify_closing_keys();
    verify_values_and_search();
    verify_characters();
    verify_mouse();
    verify_press_survives_shifted_rows();
    verify_composition_is_cancelled_on_close();
    verify_choice_opening();
    verify_choice_keys();
    verify_choice_mouse();
}
} // namespace nenenib::tests::ui
