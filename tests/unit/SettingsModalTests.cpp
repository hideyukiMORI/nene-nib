// scope `--settings-modal` の単体テスト（ADR 0170 の決定 5・6・2026-11-18 の追補 2・3・施主決定
// D102・#1828）。設定の面を開く → 切り替え → 閉じるで保存が 1 回・変えずに閉じれば書かない・
// 元に戻すで書かない・ほかの操作で閉じるときも 1 回・探す欄と焦点・節の札・組・要約・配色と字の
// 大きさがその場で frame に効く・表示言語の解決し直し・保存の失敗の知らせ。時計・ファイル・乱数の
// 種を読まない。
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "ChangeSetting.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "DisplayLanguage.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "FilterSettings.hpp"
#include "InsertText.hpp"
#include "JumpSettingsSection.hpp"
#include "OpenSettings.hpp"
#include "RevertSettings.hpp"
#include "Scopes.hpp"
#include "SettingsFailure.hpp"
#include "SettingsPreset.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "StepSettings.hpp"
#include "TestSupport.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

[[nodiscard]] const app::SettingsView &panel_of(const app::EditorFrame &frame)
{
    static const app::SettingsView empty{};
    return frame.settings_panel.has_value() ? frame.settings_panel.value() : empty;
}

// 焦点を行 subject へ送る（見えている行を下へ歩く）。
void focus_on(EditorController &controller, const core::SettingsSubject &subject)
{
    for (std::size_t step = 0; step < core::settings_rows.size(); ++step)
    {
        const auto frame = controller.frame();
        const auto &panel = panel_of(frame);
        if (panel.focused.has_value() &&
            panel.rows.at(panel.focused.value()).row.subject == subject)
        {
            return;
        }
        static_cast<void>(controller.apply(app::StepSettings{true}));
    }
}

// 開く → 切り替え → 閉じるで 1 回だけ書く。変えずに閉じれば書かない（決定 6・追補 2）。
void verify_open_change_close()
{
    Editing editing;
    auto &controller = editing.controller();
    expect(!controller.frame().settings_panel.has_value(), "the panel is closed at start");
    const auto opened = controller.apply_frame(app::OpenSettings{});
    const auto &panel = panel_of(opened);
    expect(opened.settings_panel.has_value() && panel.rows.size() == core::settings_rows.size() &&
               panel.focused == std::optional<std::size_t>{0} &&
               panel.section == std::optional{core::SettingsSection::display} && !panel.changed,
           "opening shows every row with the focus on the first");
    expect(panel.tabs.size() == 6 && panel.tabs.at(0).section == core::SettingsSection::display &&
               panel.tabs.at(5).section == core::SettingsSection::language_server &&
               panel.tabs.at(5).filled,
           "the six section tabs are listed in order");
    focus_on(controller, core::EditorFeature::minimap);
    const auto toggled = controller.apply_frame(app::ChangeSetting{true});
    expect(core::feature_enabled(toggled.settings, core::EditorFeature::minimap) &&
               toggled.minimap.has_value() && panel_of(toggled).changed &&
               editing.settings().writes() == 0,
           "switching takes effect at once without writing");
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto closed = controller.apply_frame(app::CloseSettings{});
    expect(!closed.settings_panel.has_value() && editing.settings().writes() == 1 &&
               editing.settings().written().has_value() &&
               core::feature_enabled(
                   editing.settings().written().value_or(core::default_editor_settings()),
                   core::EditorFeature::minimap),
           "closing writes the settings once");
    static_cast<void>(controller.apply(app::OpenSettings{}));
    static_cast<void>(controller.apply(app::StepSettings{true}));
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 1, "closing without a change does not write");
}

// 元に戻すは開いたときの写しへ（書かない）。切れない機能の行は何もしない。
void verify_revert()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::wrap);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    expect(!core::feature_enabled(controller.frame().settings, core::EditorFeature::wrap),
           "wrap is turned off in place");
    const auto reverted = controller.apply_frame(app::RevertSettings{});
    expect(core::feature_enabled(reverted.settings, core::EditorFeature::wrap) &&
               reverted.settings_panel.has_value() && !panel_of(reverted).changed &&
               editing.settings().writes() == 0,
           "revert returns to the copy taken at opening without writing");
    focus_on(controller, core::EditorFeature::matchparen);
    const auto fixed = controller.apply_frame(app::ChangeSetting{true});
    const auto &row = panel_of(fixed).rows.at(panel_of(fixed).focused.value_or(0));
    expect(row.on && !row.switchable && row.tier == std::optional{core::CostTier::low} &&
               !panel_of(fixed).changed,
           "a feature that cannot be turned off stays on and shows its tier");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 0, "closing after a revert does not write");
}

// ほかの使う人の操作は面を閉じて 1 回保存する。合図は面を閉じない（追補 2）。
void verify_other_intents_close()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::guide);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto signalled = controller.apply_frame(app::WorkCompleted{});
    const auto resized = controller.apply_frame(app::VisibleLines{20});
    expect(signalled.settings_panel.has_value() && resized.settings_panel.has_value() &&
               editing.settings().writes() == 0,
           "signals and window sizes keep the panel open");
    const auto typed = controller.apply_frame(app::InsertText{"a"});
    expect(!typed.settings_panel.has_value() && editing.settings().writes() == 1,
           "another user operation closes the panel and saves once");
}

// 探す欄・焦点・節の札（追補 2・施主決定 D102）。
void verify_filter_and_sections()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    const auto filtered = controller.apply_frame(app::FilterSettings{"lsp"});
    const auto &panel = panel_of(filtered);
    expect(panel.query == "lsp" && panel.rows.size() == 1 &&
               panel.rows.front().row.subject == core::SettingsSubject{core::EditorFeature::lsp} &&
               panel.focused == std::optional<std::size_t>{0} &&
               panel.section == std::optional{core::SettingsSection::language_server},
           "the search box narrows the rows and moves the focus onto a shown row");
    expect(!panel.tabs.at(0).filled && panel.tabs.at(5).filled,
           "tabs without shown rows are marked empty");
    const auto none = controller.apply_frame(app::FilterSettings{"zzzz"});
    expect(panel_of(none).rows.empty() && !panel_of(none).focused.has_value() &&
               !panel_of(none).section.has_value(),
           "nothing matches and nothing is focused");
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::FilterSettings{""}));
    const auto jumped =
        controller.apply_frame(app::JumpSettingsSection{core::SettingsSection::features});
    const auto &all = panel_of(jumped);
    expect(all.section == std::optional{core::SettingsSection::features} &&
               all.rows.at(all.focused.value_or(0)).row.subject ==
                   core::SettingsSubject{core::EditorFeature::matchparen},
           "a section tab sends the focus to the section's first row");
    const auto wrapped = controller.apply_frame(app::StepSettings{false});
    static_cast<void>(controller.apply(app::JumpSettingsSection{core::SettingsSection::display}));
    const auto top = controller.apply_frame(app::StepSettings{false});
    expect(panel_of(wrapped).section == std::optional{core::SettingsSection::panes} &&
               panel_of(top).focused == std::optional<std::size_t>{panel_of(top).rows.size() - 1},
           "stepping goes back a row and wraps around at the top");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 0, "searching and moving do not change settings");
}

// 組と要約（決定 5・追補 3）。
void verify_presets()
{
    Editing editing;
    auto &controller = editing.controller();
    const auto opened = controller.apply_frame(app::OpenSettings{});
    expect(panel_of(opened).summary.preset == std::optional{core::SettingsPreset::recommended} &&
               panel_of(opened).summary.estimate_us == core::reference_single_key_us + 17,
           "the default settings show the recommended preset and the matchparen estimate");
    const auto all =
        controller.apply_frame(app::ChooseSettingsPreset{core::SettingsPreset::all_on});
    expect(all.settings.lsp &&
               panel_of(all).summary.preset == std::optional{core::SettingsPreset::all_on} &&
               panel_of(all).summary.heavy.size() == 1,
           "the all-on preset turns the language server on");
    const auto speed =
        controller.apply_frame(app::ChooseSettingsPreset{core::SettingsPreset::speed_first});
    expect(!speed.settings.lsp && !panel_of(speed).changed,
           "the speed-first preset turns it off again");
    const auto &rows = panel_of(speed).rows;
    expect(std::ranges::count_if(rows, [](const app::SettingsRowView &row)
                                 { return row.background; }) == 1,
           "only the measured background feature carries the background mark");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 0, "presets that end where they began do not write");
}

// 配色・字の大きさ・表示言語はその場で frame に効く（追補 2）。
void verify_immediate_look()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto themed = controller.apply_frame(app::ChangeSetting{true});
    const auto light = core::theme_of(core::BuiltinTheme::neutral_light);
    expect(themed.palette.background == light.ui.background &&
               panel_of(themed).rows.front().value == light.name,
           "the color scheme changes the frame's palette at once");
    focus_on(controller, core::SettingsValue::font_size);
    const auto bigger = controller.apply_frame(app::ChangeSetting{true});
    expect(bigger.settings.font_size.points() == 14.5F, "the font size changes at once");
    focus_on(controller, core::SettingsValue::ui_language);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto english = controller.apply_frame(app::ChangeSetting{true});
    expect(english.language == core::DisplayLanguage::english,
           "the display language is resolved again at once");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 1, "all of it is written once at closing");
}

// 保存の失敗は 1 行の知らせと状態の失敗（ADR 0114 の規則）。設定は変えたまま。
void verify_failure()
{
    using nenenib::application::SettingsFailure;
    Editing editing;
    auto &controller = editing.controller();
    editing.settings().fail(SettingsFailure::unwritable);
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::guide);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto closed = controller.apply_frame(app::CloseSettings{});
    expect(closed.settings_failure == SettingsFailure::unwritable &&
               closed.command_message.has_value() &&
               closed.command_message.value().text() ==
                   core::ui_text(core::UiMessage::settings_unsaved, closed.language) &&
               !core::feature_enabled(closed.settings, core::EditorFeature::guide),
           "a failed save is told in one line and keeps the changed settings");
    editing.settings().fail(std::nullopt);
    static_cast<void>(controller.apply(app::OpenSettings{}));
    const auto retried = controller.apply_frame(app::CloseSettings{});
    expect(editing.settings().writes() == 2 && !retried.settings_failure.has_value(),
           "closing again after a failure retries the write");
}
} // namespace

void verify_settings_modal_scope()
{
    verify_open_change_close();
    verify_revert();
    verify_other_intents_close();
    verify_filter_and_sections();
    verify_presets();
    verify_immediate_look();
    verify_failure();
}
} // namespace nenenib::tests
