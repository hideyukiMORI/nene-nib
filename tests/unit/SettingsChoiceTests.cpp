// #1926（採用案 2026-11-24）: 設定の選ぶ欄を一覧から選ぶ形の core と application の
// 契約。選択肢の列は 1 つずつ送る列（settings_value_stepped）と同じ順・名前は値の行の
// 表示と同じ・頭文字の跳び・一覧の配置（下・入らなければ上・最大 10 行・右端を箱に
// そろえる・送り）と当たり・開く・選ぼうとしている位置・決める（即時に反映・閉じると
// 保存・元に戻す）・決めずに閉じる・打った字を 1 秒の合図で忘れる・合図は一覧を閉じない・
// ほかの操作は一覧を決めずに閉じる。時計・ファイル・乱数の種を読まない。
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "ChooseSettingsChoice.hpp"
#include "CloseSettings.hpp"
#include "CloseSettingsChoice.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FocusSettingsRow.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "NewTab.hpp"
#include "OpenSettings.hpp"
#include "OpenSettingsChoice.hpp"
#include "PointSettingsChoice.hpp"
#include "RevertSettings.hpp"
#include "Scopes.hpp"
#include "ScrollSettings.hpp"
#include "ScrollSettingsChoice.hpp"
#include "SelectEditMode.hpp"
#include "SettingsChoiceLayout.hpp"
#include "SettingsChoiceRequest.hpp"
#include "SettingsChoiceTimeout.hpp"
#include "SettingsChoiceView.hpp"
#include "SettingsChoices.hpp"
#include "SettingsControl.hpp"
#include "SettingsPanel.hpp"
#include "SettingsRows.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "SettleRecentTab.hpp"
#include "StepSettings.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "ThemeChoice.hpp"
#include "ThemeDocument.hpp"
#include "ThemeName.hpp"
#include "TypeSettingsChoice.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::uint32_t dpi = 96;

// 面の表の値の行のうち、部品が選ぶ欄（choice）の値の列（手で並べない・レビュー M1）。
[[nodiscard]] std::vector<core::SettingsValue> choice_values()
{
    std::vector<core::SettingsValue> values;
    for (const auto &row : core::settings_rows)
    {
        if (std::holds_alternative<core::SettingsValue>(row.subject) &&
            core::settings_control_of(row.subject, false) == core::SettingsControl::choice)
        {
            values.push_back(std::get<core::SettingsValue>(row.subject));
        }
    }
    return values;
}

// 表の値の行の全部で「部品が選ぶ欄」と「一覧が 2 つ以上の選択肢を持つ」が一致する（レビュー M1）。
// 値を choice にして一覧の case を足し忘れる（数 0）と、Enter と値の箱のクリックが何もしなくなる。
void verify_choice_rows_have_lists()
{
    const auto settings = core::default_editor_settings();
    bool matched = true;
    std::size_t choices = 0;
    for (const auto &row : core::settings_rows)
    {
        if (!std::holds_alternative<core::SettingsValue>(row.subject))
        {
            continue;
        }
        const auto value = std::get<core::SettingsValue>(row.subject);
        const bool choice =
            core::settings_control_of(row.subject, false) == core::SettingsControl::choice;
        const auto count = core::settings_choice_count(settings, value, std::nullopt);
        matched = matched && choice == (count >= 2) && (choice || count == 0);
        choices += choice ? 1U : 0U;
    }
    expect(matched && choices > 0,
           "every choice value row has a list of two or more and no other value row has one");
}

[[nodiscard]] bool same_value(const core::EditorSettings &left, const core::EditorSettings &right,
                              core::SettingsValue value)
{
    return core::settings_value_text(left, value, core::DisplayLanguage::english) ==
           core::settings_value_text(right, value, core::DisplayLanguage::english);
}

// 列の順は 1 つずつ送る列と同じ（位置 p の値から次へ送ると位置 p + 1 の値・回り込む）で、名前は値の
// 行の表示と同じ。
void verify_lists_follow_the_steps()
{
    const auto settings = core::default_editor_settings();
    bool ordered = true;
    bool named = true;
    for (const auto value : choice_values())
    {
        const auto count = core::settings_choice_count(settings, value, std::nullopt);
        const auto texts = core::settings_choice_texts(settings, value, std::nullopt,
                                                       core::DisplayLanguage::japanese);
        ordered = ordered && count >= 2 && texts.size() == count;
        for (std::size_t at = 0; at < count; ++at)
        {
            const auto chosen = core::settings_choice_applied(settings, value, at, std::nullopt);
            const auto next = core::settings_value_stepped(chosen, value, true, std::nullopt);
            const auto expected =
                core::settings_choice_applied(settings, value, (at + 1) % count, std::nullopt);
            ordered =
                ordered && same_value(next, expected, value) &&
                core::settings_choice_position(chosen, value, std::nullopt) == std::optional{at};
            named = named && texts.at(at) == core::settings_value_text(
                                                 chosen, value, core::DisplayLanguage::japanese);
        }
    }
    expect(ordered, "each choice list has the order the arrows step through");
    expect(named, "each choice is named as the value row shows it");
    expect(core::settings_choice_count(settings, core::SettingsValue::theme, std::nullopt) ==
               core::builtin_themes.size() + 1,
           "the color schemes list Windows first and then every built-in scheme");
    expect(
        core::settings_choice_count(settings, core::SettingsValue::font_size, std::nullopt) == 0 &&
            core::settings_choice_count(settings, core::SettingsValue::font_family, std::nullopt) ==
                0 &&
            !core::settings_choice_position(settings, core::SettingsValue::font_size, std::nullopt)
                 .has_value(),
        "the stepper and read-only rows have no choice list");
}

// 一覧に無い配色（利用者のテーマ）は列の最後に 1 つ（今の値か、面を開いたときの値なら）。
void verify_outside_scheme()
{
    const auto &base = core::theme_of(core::BuiltinTheme::neutral_light);
    const auto mine = std::optional{core::ThemeChoice::from(
        core::ThemeDocument{core::ThemeName::parse("my-own-theme").value(),
                            base.appearance,
                            base.ui,
                            base.body,
                            {fixed_text("author"), fixed_text("MIT"), fixed_text("local")}})};
    auto settings = core::default_editor_settings();
    settings.theme = mine;
    const auto count = core::settings_choice_count(settings, core::SettingsValue::theme, mine);
    const auto texts = core::settings_choice_texts(settings, core::SettingsValue::theme, mine,
                                                   core::DisplayLanguage::english);
    expect(count == core::builtin_themes.size() + 2 && texts.back() == "my-own-theme" &&
               core::settings_choice_position(settings, core::SettingsValue::theme, mine) ==
                   std::optional{count - 1},
           "a user theme is the last choice and is the current one");
}

void verify_typed_jump()
{
    const std::vector<std::string> texts{"Windows", "one-dark", "Solarized-dark",
                                         "solarized-light"};
    expect(core::settings_choice_typed(texts, "sol") == std::optional<std::size_t>{2} &&
               core::settings_choice_typed(texts, "SOLARIZED-L") == std::optional<std::size_t>{3} &&
               core::settings_choice_typed(texts, "o") == std::optional<std::size_t>{1} &&
               !core::settings_choice_typed(texts, "x").has_value() &&
               !core::settings_choice_typed(texts, "").has_value(),
           "typing jumps to the first choice that starts with the typed letters");
}

[[nodiscard]] core::SettingsChoiceLayout arranged(core::LayoutRect anchor, std::size_t count,
                                                  std::size_t top, std::int32_t widest)
{
    return core::settings_choice_layout(core::SettingsChoiceRequest{
        anchor, core::LayoutRect{100, 0, 560, 800}, dpi, count, top, widest});
}

void verify_layout()
{
    const core::LayoutRect box{340, 100, 520, 128};
    const auto below = arranged(box, 126, 0, 60);
    const auto row = core::settings_choice_row(below, 0);
    expect(!below.above && below.rows == core::settings_choice_rows &&
               below.panel.top == box.bottom + 4 && below.panel.right == box.right &&
               core::width_of(below.panel) == core::width_of(box) && core::height_of(row) == 30 &&
               row.top == below.panel.top + 4 && core::height_of(below.panel) == 4 + (30 * 10) + 4,
           "the list opens under the box with its right edge on the box and ten rows at most");
    const auto wide = arranged(box, 3, 0, 400);
    expect(wide.rows == 3 && wide.panel.right == box.right && wide.panel.left == 100 &&
               core::width_of(core::settings_choice_thumb(wide)) == 0,
           "a long name widens the list up to the panel and a short list has no scroll bar");
    const core::LayoutRect low{340, 640, 520, 668};
    const auto above = arranged(low, 126, 0, 60);
    expect(above.above && above.rows == 10 && above.panel.bottom == low.top - 4,
           "the list opens above the box when it does not fit below");
    const auto clamped = arranged(box, 126, 200, 60);
    expect(clamped.first == 116 && core::width_of(core::settings_choice_thumb(clamped)) > 0 &&
               core::settings_choice_thumb(clamped).bottom == clamped.panel.bottom - 4,
           "the scroll stops where the last ten choices are shown");
    const auto first_row = core::settings_choice_row(below, 0);
    expect(
        core::settings_choice_hit(below, first_row.left + 4, first_row.top + 2) ==
                std::optional<std::size_t>{0} &&
            core::settings_choice_hit(below, first_row.left + 4, first_row.top + 30 * 3 + 1) ==
                std::optional<std::size_t>{3} &&
            !core::settings_choice_hit(below, first_row.left + 4, below.panel.top + 1)
                 .has_value() &&
            !core::settings_choice_hit(below, below.panel.left - 1, first_row.top + 2).has_value(),
        "the hit names the choice under the point and nothing in the gaps or outside");
    expect(core::settings_choice_revealing(below, 12) == 3 &&
               core::settings_choice_revealing(clamped, 4) == 4 &&
               core::settings_choice_revealing(below, 5) == 0,
           "the scroll reveals a choice that is out of sight and keeps a shown one");
    expect(core::settings_choice_centred(126, 60) == 55 &&
               core::settings_choice_centred(126, 2) == 0 &&
               core::settings_choice_centred(126, 125) == 116 &&
               core::settings_choice_centred(4, 3) == 0,
           "opening scrolls the current choice to the middle of the shown rows");
    expect(core::settings_choice_paged(126, 0, true, 0) == 9 &&
               core::settings_choice_paged(126, 120, true, 10) == 125 &&
               core::settings_choice_paged(126, 5, false, 10) == 0 &&
               core::settings_choice_paged(2, 0, true, 0) == 1 &&
               core::settings_choice_paged(126, 0, true, 3) == 2,
           "a page moves by the shown rows less one (fewer in a low window) and stops at the ends");
    const auto bar = core::settings_choice_thumb(below);
    const auto wide_row = core::settings_choice_row(wide, 0);
    expect(!core::settings_choice_hit(below, bar.left, bar.top + 1).has_value() &&
               core::settings_choice_hit(wide, wide.panel.right - 2, wide_row.top + 2) ==
                   std::optional<std::size_t>{0},
           "the scroll bar column picks no choice and a list without a bar picks to its edge");
    expect(core::settings_choice_scrolled(below, 0, 3) == std::optional<std::size_t>{3} &&
               core::settings_choice_scrolled(below, 114, 5) == std::optional<std::size_t>{116} &&
               !core::settings_choice_scrolled(below, 116, 2).has_value() &&
               !core::settings_choice_scrolled(below, 0, -1).has_value() &&
               !core::settings_choice_scrolled(wide, 0, 3).has_value(),
           "the wheel scrolls the list from the state's scroll and stops at the ends");
}

[[nodiscard]] const app::SettingsView &panel_of(const app::EditorFrame &frame)
{
    static const app::SettingsView empty{};
    return frame.settings_panel.has_value() ? frame.settings_panel.value() : empty;
}

[[nodiscard]] bool listed(const app::EditorFrame &frame)
{
    return panel_of(frame).choice.has_value();
}

[[nodiscard]] const app::SettingsChoiceView &choice_of(const app::EditorFrame &frame)
{
    static const app::SettingsChoiceView none{{}, std::nullopt, 0, 0, {}};
    const auto &choice = panel_of(frame).choice;
    return choice.has_value() ? choice.value() : none;
}

// 配色の行（表の先頭）に焦点を置いて面を開く。
void open_on_theme(EditorController &controller)
{
    static_cast<void>(controller.apply(app::OpenSettings{}));
    static_cast<void>(controller.apply(app::FocusSettingsRow{0}));
}

void verify_open_and_choose()
{
    Editing editing;
    auto &controller = editing.controller();
    open_on_theme(controller);
    const auto before = controller.frame().settings;
    const auto opened = controller.apply_frame(app::OpenSettingsChoice{});
    const auto &choice = choice_of(opened);
    const auto current =
        core::settings_choice_position(before, core::SettingsValue::theme, before.theme);
    expect(listed(opened) && choice.choices.size() == core::builtin_themes.size() + 1 &&
               choice.current == current && choice.candidate == current.value_or(0) &&
               choice.top == core::settings_choice_centred(choice.choices.size(), choice.candidate),
           "opening lists the schemes with the current one chosen and centred");
    const auto pointed = controller.apply_frame(app::PointSettingsChoice{60});
    expect(choice_of(pointed).candidate == 60 &&
               same_value(pointed.settings, before, core::SettingsValue::theme),
           "pointing at a choice does not change the scheme before it is chosen");
    const auto chosen = controller.apply_frame(app::ChooseSettingsChoice{});
    const auto wanted =
        core::settings_choice_applied(before, core::SettingsValue::theme, 60, before.theme);
    expect(!listed(chosen) && same_value(chosen.settings, wanted, core::SettingsValue::theme) &&
               panel_of(chosen).focused == std::optional<std::size_t>{0} &&
               panel_of(chosen).changed && editing.settings().writes() == 0,
           "choosing takes effect at once, closes the list and keeps the focus without writing");
    const auto again = controller.apply_frame(app::OpenSettingsChoice{});
    expect(choice_of(again).current == std::optional<std::size_t>{60} && choice_of(again).top == 55,
           "reopening starts from the chosen scheme in the middle of the list");
    static_cast<void>(controller.apply(app::CloseSettingsChoice{}));
    const auto reverted = controller.apply_frame(app::RevertSettings{});
    expect(same_value(reverted.settings, before, core::SettingsValue::theme),
           "revert undoes a choice from the list");
    static_cast<void>(controller.apply(app::OpenSettingsChoice{}));
    static_cast<void>(controller.apply(app::PointSettingsChoice{61}));
    static_cast<void>(controller.apply(app::ChooseSettingsChoice{}));
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 1 && editing.settings().written().has_value() &&
               same_value(editing.settings().written().value_or(core::default_editor_settings()),
                          core::settings_choice_applied(before, core::SettingsValue::theme, 61,
                                                        before.theme),
                          core::SettingsValue::theme),
           "closing the panel writes the chosen scheme once");
}

void verify_cancel_and_bounds()
{
    Editing editing;
    auto &controller = editing.controller();
    open_on_theme(controller);
    const auto before = controller.frame().settings;
    static_cast<void>(controller.apply(app::OpenSettingsChoice{}));
    static_cast<void>(controller.apply(app::PointSettingsChoice{9999}));
    expect(choice_of(controller.frame()).candidate == core::builtin_themes.size(),
           "pointing past the end stops at the last choice");
    static_cast<void>(controller.apply(app::ScrollSettingsChoice{9999}));
    expect(choice_of(controller.frame()).top == core::builtin_themes.size(),
           "the scroll is held within the list");
    const auto cancelled = controller.apply_frame(app::CloseSettingsChoice{});
    expect(!listed(cancelled) && cancelled.settings_panel.has_value() &&
               same_value(cancelled.settings, before, core::SettingsValue::theme),
           "closing the list without choosing keeps the scheme and the panel");
    static_cast<void>(controller.apply(app::FocusSettingsRow{2}));
    expect(!listed(controller.apply_frame(app::OpenSettingsChoice{})),
           "a switch row has no list to open");
    static_cast<void>(controller.apply(app::ChooseSettingsChoice{}));
    static_cast<void>(controller.apply(app::PointSettingsChoice{1}));
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 0 &&
               !listed(controller.apply_frame(app::OpenSettingsChoice{})),
           "the list intents do nothing while no list or no panel is open");
}

void verify_typing_and_signals()
{
    Editing editing;
    auto &controller = editing.controller();
    open_on_theme(controller);
    static_cast<void>(controller.apply(app::OpenSettingsChoice{}));
    const auto texts = choice_of(controller.frame()).choices;
    const auto typed = controller.apply_frame(app::TypeSettingsChoice{"sol"});
    expect(choice_of(typed).candidate == core::settings_choice_typed(texts, "sol").value_or(0) &&
               choice_of(typed).typed == "sol",
           "typed letters jump to the first scheme that starts with them");
    const auto missed = controller.apply_frame(app::TypeSettingsChoice{"solzz"});
    expect(choice_of(missed).candidate == choice_of(typed).candidate,
           "letters that match nothing leave the chosen row where it was");
    const auto waited = controller.apply_frame(app::WorkCompleted{});
    expect(listed(waited) && choice_of(waited).typed == "solzz",
           "a background signal neither closes the list nor forgets the letters");
    const auto forgotten = controller.apply_frame(app::SettingsChoiceTimeout{});
    expect(listed(forgotten) && choice_of(forgotten).typed.empty() &&
               choice_of(forgotten).candidate == choice_of(typed).candidate,
           "the one second signal forgets the letters and keeps the list");
    const auto scrolled = controller.apply_frame(app::ScrollSettings{0});
    expect(listed(scrolled), "following the focus row with the panel scroll keeps the list");
    const auto stepped = controller.apply_frame(app::StepSettings{true});
    expect(!listed(stepped) && stepped.settings_panel.has_value(),
           "another panel intent closes the list without choosing");
    static_cast<void>(controller.apply(app::FocusSettingsRow{0}));
    static_cast<void>(controller.apply(app::OpenSettingsChoice{}));
    static_cast<void>(controller.apply(app::InsertText{"x"}));
    expect(!controller.frame().settings_panel.has_value() && editing.settings().writes() == 0,
           "typing in the text closes the panel and the list without a choice to save");
}

// 頭文字の 1 秒の合図は合図（is_signal）の側で、Ctrl+Tab の歩きも 1
// 意図の知らせも動かさない（レビュー R4 a・`map` の待ちは KeyMapTests の
// verify_flush_before_intents）。
void verify_timeout_is_a_signal()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{10}));
    static_cast<void>(controller.apply(app::InsertText{"a"}));
    for (const std::string_view body : {"b", "c", "d"})
    {
        static_cast<void>(controller.apply(app::NewTab{}));
        static_cast<void>(controller.apply(app::InsertText{std::string(body)}));
    }
    static_cast<void>(controller.apply(app::WalkRecentTab{core::TabStep::next}));
    static_cast<void>(controller.apply(app::SettingsChoiceTimeout{}));
    expect(controller.tab_walking(), "the typing signal does not settle the Ctrl+Tab walk");
    static_cast<void>(controller.apply(app::SettleRecentTab{}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    const auto rejected = run_ex(controller, "tabnext 9");
    const auto after = controller.apply_frame(app::SettingsChoiceTimeout{});
    expect(rejected.command_message.has_value() && after.command_message.has_value(),
           "the typing signal keeps the one-intent notice");
}
} // namespace

void verify_settings_choice_scope()
{
    verify_choice_rows_have_lists();
    verify_timeout_is_a_signal();
    verify_lists_follow_the_steps();
    verify_outside_scheme();
    verify_typed_jump();
    verify_layout();
    verify_open_and_choose();
    verify_cancel_and_bounds();
    verify_typing_and_signals();
}
} // namespace nenenib::tests
