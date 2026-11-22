// scope `--settings-modal` の単体テスト（ADR 0170 の決定 5・6・2026-11-18 の追補 2・3・施主決定
// D102・#1828）。設定の面を開く → 切り替え → 閉じるで保存が 1 回・変えずに閉じれば書かない・
// 元に戻すで書かない・ほかの操作で閉じるときも 1 回・探す欄と焦点・節の札・組・要約・配色と字の
// 大きさがその場で frame に効く・表示言語の解決し直し・保存の失敗の知らせ。時計・ファイル・乱数の
// 種を読まない。
#include "AdjustFontSize.hpp"
#include "AutosaveTick.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "ChangeSetting.hpp"
#include "ChooseSettingsPreset.hpp"
#include "CloseSettings.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "CommandText.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "EndSession.hpp"
#include "FeatureCosts.hpp"
#include "FilePath.hpp"
#include "FilterSettings.hpp"
#include "FirstFramePresented.hpp"
#include "FontSize.hpp"
#include "FontSizeAdjustment.hpp"
#include "InsertText.hpp"
#include "JumpSettingsSection.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageServerTick.hpp"
#include "LineNumber.hpp"
#include "MapTimeout.hpp"
#include "MinimapPointing.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenCompletion.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "PlaceCaret.hpp"
#include "PointMinimap.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewTick.hpp"
#include "QueryLanguage.hpp"
#include "RevertSettings.hpp"
#include "Scopes.hpp"
#include "ScrollExplorer.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "ScrollTabs.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SessionEnd.hpp"
#include "SetPreviewPlacement.hpp"
#include "SettingsFailure.hpp"
#include "SettingsPreset.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsValue.hpp"
#include "SettingsView.hpp"
#include "StepSettings.hpp"
#include "SubmitCommand.hpp"
#include "TabTipTick.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
    // 言語サーバーの節の行（#1945）も歩くので、設定の行の数より多く送れるようにする。
    for (std::size_t step = 0; step < core::settings_rows.size() * 4; ++step)
    {
        const auto frame = controller.frame();
        const auto &panel = panel_of(frame);
        if (panel.focused.has_value() && panel.focused.value() < panel.rows.size() &&
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
               panel.section == std::optional{core::SettingsSection::features},
           "the search box narrows the rows and moves the focus onto a shown row");
    expect(!panel.tabs.at(0).filled && panel.tabs.at(4).filled && !panel.tabs.at(5).filled &&
               !panel.lsp_listed,
           "tabs without shown rows are marked empty and the command lines hide while filtering");
    const auto none = controller.apply_frame(app::FilterSettings{"zzzz"});
    expect(panel_of(none).rows.empty() && !panel_of(none).focused.has_value() &&
               !panel_of(none).section.has_value(),
           "nothing matches and nothing is focused");
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::FilterSettings{""}));
    // 言語サーバーの節は言語ごとの行を持つ（#1945）。札は最初の行（C / C++）へ焦点を送る。
    const auto served =
        controller.apply_frame(app::JumpSettingsSection{core::SettingsSection::language_server});
    expect(panel_of(served).lsp_listed && panel_of(served).tabs.at(5).filled &&
               panel_of(served).focused ==
                   std::optional<std::size_t>{panel_of(served).rows.size()} &&
               panel_of(served).section == std::optional{core::SettingsSection::language_server},
           "the language server tab sends the focus to its first language row");
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
    const auto &last = panel_of(top);
    const std::size_t total =
        last.rows.size() + last.servers.size() + (last.servers_hidden > 0 ? 1 : 0);
    expect(panel_of(wrapped).section == std::optional{core::SettingsSection::panes} &&
               last.focused == std::optional<std::size_t>{total - 1} &&
               last.section == std::optional{core::SettingsSection::language_server},
           "stepping goes back a row and wraps around at the top to the last language row");
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
// 全面プレビューの間も面は開き、切り替えて閉じれば 1 回書く（#1835 のレビュー M1）。
void verify_full_preview()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at(std::string("C:\\work\\probe.md"), Bytes{std::string("# t\n\ntext\n")});
    static_cast<void>(controller.apply(app::VisibleLines{20, {}, 1000, 700}));
    static_cast<void>(
        controller.apply(app::OpenDocument{core::FilePath::parse("C:\\work\\probe.md").value()}));
    const auto full = controller.apply_frame(app::SetPreviewPlacement{app::PreviewPlacement::full});
    expect(full.full_preview.has_value(), "the full preview is shown");
    const auto opened = controller.apply_frame(app::OpenSettings{});
    expect(opened.settings_panel.has_value(), "the panel opens over the full preview");
    focus_on(controller, core::EditorFeature::guide);
    const auto changed = controller.apply_frame(app::ChangeSetting{true});
    const auto closed = controller.apply_frame(app::CloseSettings{});
    expect(!core::feature_enabled(changed.settings, core::EditorFeature::guide) &&
               !closed.settings_panel.has_value() && closed.full_preview.has_value() &&
               editing.settings().writes() == 1,
           "switching and closing work over the full preview and write once");
}

// 合図 6 つでは閉じず書かない。窓を閉じる EndSession は閉じて 1 回だけ書き、2 回目は書かない
// （R7）。タブを閉じるのもほかの使う人の操作として閉じて 1 回書く。
void verify_signals_and_session()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::guide);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::AutosaveTick{}));
    static_cast<void>(controller.apply(app::LanguageServerTick{}));
    static_cast<void>(controller.apply(app::MapTimeout{}));
    static_cast<void>(controller.apply(app::PreviewTick{}));
    static_cast<void>(controller.apply(app::FirstFramePresented{}));
    static_cast<void>(controller.apply(app::TabTipTick{true}));
    expect(controller.settings_open() && editing.settings().writes() == 0,
           "the six signals keep the panel open and write nothing");
    static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
    expect(!controller.settings_open() && editing.settings().writes() == 1,
           "closing the window closes the panel and writes once");
    static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
    expect(editing.settings().writes() == 1, "a second EndSession does not write");

    Editing tabs;
    auto &other = tabs.controller();
    static_cast<void>(other.apply(app::NewTab{}));
    static_cast<void>(other.apply(app::OpenSettings{}));
    focus_on(other, core::EditorFeature::guide);
    static_cast<void>(other.apply(app::ChangeSetting{true}));
    static_cast<void>(other.apply(app::CloseTab{1}));
    expect(!other.settings_open() && tabs.settings().writes() == 1,
           "closing a tab closes the panel and writes once");
}

// 閉じるときの保存に失敗したら、窓を閉じるときにもう 1 回だけ書く（R1）。2 回目も失敗すれば失敗を
// 状態に置くだけで、さらには書かない。
void verify_retry_at_session_end()
{
    using nenenib::application::SettingsFailure;
    {
        Editing editing;
        auto &controller = editing.controller();
        editing.settings().fail(SettingsFailure::unwritable);
        static_cast<void>(controller.apply(app::OpenSettings{}));
        focus_on(controller, core::EditorFeature::guide);
        static_cast<void>(controller.apply(app::ChangeSetting{true}));
        static_cast<void>(controller.apply(app::CloseSettings{}));
        expect(editing.settings().writes() == 1, "the failed close tried once");
        editing.settings().fail(std::nullopt);
        static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
        expect(editing.settings().writes() == 2 && editing.settings().written().has_value() &&
                   !core::feature_enabled(
                       editing.settings().written().value_or(core::default_editor_settings()),
                       core::EditorFeature::guide),
               "closing the window writes the unsaved settings once more");
        static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
        expect(editing.settings().writes() == 2, "after the retry nothing is left to write");
    }
    {
        Editing editing;
        auto &controller = editing.controller();
        editing.settings().fail(SettingsFailure::unwritable);
        static_cast<void>(controller.apply(app::OpenSettings{}));
        focus_on(controller, core::EditorFeature::guide);
        static_cast<void>(controller.apply(app::ChangeSetting{true}));
        static_cast<void>(controller.apply(app::CloseSettings{}));
        const auto ended = controller.apply_frame(app::EndSession{app::SessionEnd::window_closed});
        static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
        expect(editing.settings().writes() == 2 &&
                   ended.settings_failure == SettingsFailure::unwritable,
               "a second failure is kept as the settings failure and is not retried again");
    }
}

// 開くときに Ctrl+P の面と入力行を閉じる（配色の仮当てごと・R2）。面が開いている間に Ctrl+P を
// 開く操作は、面を閉じて保存してから開く。
void verify_open_closes_input()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"colorscheme neutral"}));
    const auto opened = controller.apply_frame(app::OpenSettings{});
    expect(opened.settings_panel.has_value() && !opened.command_palette.has_value() &&
               !opened.command_line.has_value(),
           "opening the panel closes the Ctrl+P panel and the input line");
    focus_on(controller, core::EditorFeature::guide);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto palette = controller.apply_frame(app::OpenCommandPalette{});
    expect(!palette.settings_panel.has_value() && palette.command_palette.has_value() &&
               editing.settings().writes() == 1,
           "opening Ctrl+P while the panel is open closes and saves it first");
}

// 本文や面を送るだけの意図は面を閉じない。本文のクリックは閉じて保存する（R8）。
void verify_scrolls_keep_open()
{
    Editing editing;
    auto &controller = editing.controller();
    std::string text;
    for (int line = 0; line < 50; ++line)
    {
        text += "line\n";
    }
    static_cast<void>(controller.apply(app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}}));
    static_cast<void>(controller.apply(app::InsertText{text}));
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::guide);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto scrolled = controller.apply_frame(app::ScrollLines{-5});
    static_cast<void>(controller.apply(app::ScrollPane{3}));
    static_cast<void>(controller.apply(app::ScrollExplorer{2}));
    static_cast<void>(controller.apply(app::ScrollTabs{1}));
    static_cast<void>(controller.apply(app::PointMinimap{app::MinimapPointing::pressed, 0}));
    expect(scrolled.settings_panel.has_value() && controller.settings_open() &&
               editing.settings().writes() == 0,
           "scrolling the body, the other pane, the explorer, the tabs and the minimap keeps the "
           "panel open");
    // Ctrl+ホイールの字の大きさも面を閉じない。大きさの行の値がその場で変わり、書くのは閉じるときの
    // 1 回（#1836 の差し戻し）。
    const auto before = controller.frame().settings.font_size;
    const auto zoomed =
        controller.apply_frame(app::AdjustFontSize{core::FontSizeAdjustment::increase, 2});
    focus_on(controller, core::SettingsValue::font_size);
    const auto sized = controller.frame();
    const auto &size_row = panel_of(sized).rows.at(panel_of(sized).focused.value_or(0));
    expect(zoomed.settings_panel.has_value() && controller.settings_open() &&
               zoomed.settings.font_size.points() != before.points() &&
               editing.settings().writes() == 0 &&
               size_row.value == core::font_size_text(zoomed.settings.font_size),
           "Ctrl+wheel changes the font size in place without closing or writing");
    static_cast<void>(controller.apply(
        app::PointText{10.0F, 0, core::SelectionAnchoring::collapse, app::PointerClicks::single}));
    expect(!controller.settings_open() && editing.settings().writes() == 1,
           "a click in the body closes the panel and saves");
}

// 設定の面と補完の面・hover の札・定義の問いは同時に出さない（ADR 0190 の決定 5・6 と ADR 0170 の
// 面どうしの排他）。設定の面を開くと補完の面は閉じ、設定の面が開いている間の補完と問いの意図は、
// 面を閉じるだけで補完の面も問いも出さない（窓は面が開いている間 Ctrl+Space と F12 を捨てる）。
void verify_faces_are_exclusive()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::InsertText{"alpha beta alto al"}));
    const auto face = controller.apply_frame(app::OpenCompletion{});
    const auto panel = controller.apply_frame(app::OpenSettings{});
    expect(face.completion.has_value() && !panel.completion.has_value() &&
               !controller.completion_open() && panel.settings_panel.has_value(),
           "opening the settings panel closes the completion face");
    const auto refused = controller.apply_frame(app::OpenCompletion{});
    expect(!refused.settings_panel.has_value() && !refused.completion.has_value() &&
               !controller.completion_open() && !refused.command_message.has_value() &&
               editing.settings().writes() == 0,
           "a completion intent while the panel is open only closes the panel");
    static_cast<void>(controller.apply(app::OpenSettings{}));
    const auto asked =
        controller.apply_frame(app::QueryLanguage{core::LanguageQueryKind::definition});
    expect(!asked.settings_panel.has_value() && !asked.command_message.has_value() &&
               editing.language_server().touches() == 0,
           "a language query while the panel is open only closes the panel");
    const auto reopened = controller.apply_frame(app::OpenCompletion{});
    expect(reopened.completion.has_value(),
           "the completion face opens again once the panel is closed");
}

// 言語サーバーを面で切り替えると、`:set lsp` と同じく起動と停止の道を通る（R7）。
void verify_language_server_switch()
{
    auto settings = core::default_editor_settings();
    settings.lsp_commands = {{"cpp", std::string{R"("C:\LLVM\bin\clangd.exe" --log=error)"}}};
    Editing editing{SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}}};
    auto &controller = editing.controller();
    editing.files().hold_at(std::string("C:\\work\\main.cpp"),
                            Bytes{std::string("int main() {}\n")});
    static_cast<void>(
        controller.apply(app::OpenDocument{core::FilePath::parse("C:\\work\\main.cpp").value()}));
    auto &server = editing.language_server();
    expect(server.launches().empty(), "the server is off at start");
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::lsp);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    expect(server.launches().size() == 1 && editing.settings().writes() == 0,
           "turning the server on in the panel starts it without saving");
    static_cast<void>(controller.apply(app::RevertSettings{}));
    expect(server.stops() == 1 && !controller.frame().settings.lsp,
           "reverting turns it off and stops it");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 0, "nothing is written after the revert");
}

// 60 行・8 行の窓・so=4 の Vim の窓（VimViewTests の #1524 の窓と同じ）で `3<C-e>` の後の窓。
[[nodiscard]] std::pair<std::size_t, std::size_t> wrap_switched(bool through_panel)
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::VisibleLines{8, core::TextViewGeometry{400, 20, 96}}));
    std::string text;
    for (std::size_t number = 1; number <= 60; ++number)
    {
        text += std::string(number < 10 ? "0" : "") + std::to_string(number) +
                (number < 60 ? "\n" : "");
    }
    static_cast<void>(controller.apply(app::InsertText{text}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(run_ex(controller, "set so=4"));
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{57}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    const auto current = static_cast<std::int64_t>(controller.frame().first_visible.value);
    static_cast<void>(controller.apply(app::ScrollLines{static_cast<std::int32_t>(53 - current)}));
    for (const auto &key : vim_keys_of("3<C-e>"))
    {
        static_cast<void>(controller.apply(app::VimKeyPress{key}));
    }
    if (through_panel)
    {
        static_cast<void>(controller.apply(app::OpenSettings{}));
        focus_on(controller, core::EditorFeature::wrap);
        static_cast<void>(controller.apply(app::ChangeSetting{true}));
        static_cast<void>(controller.apply(app::CloseSettings{}));
    }
    else
    {
        static_cast<void>(run_ex(controller, "set nowrap"));
    }
    for (const auto &key : vim_keys_of("j"))
    {
        static_cast<void>(controller.apply(app::VimKeyPress{key}));
    }
    const auto frame = controller.frame();
    return {frame.first_visible.value, frame.caret.position.line.value};
}

// Vim のモードで面から wrap を切り替えたときの窓の決め直しは `:set nowrap` と同じ（レビュー L1・
// #1524）。
void verify_vim_wrap_window()
{
    const auto by_ex = wrap_switched(false);
    expect(by_ex == std::pair<std::size_t, std::size_t>{55, 60},
           "#1524 :set nowrap re-decides the window before the next key");
    expect(wrap_switched(true) == by_ex,
           "switching wrap in the panel re-decides the Vim window like :set nowrap");
}

// 面が開いたまま窓を閉じて書けないとき、同じ EndSession の中で書こうとするのは 1 回だけ（#1835 の
// 再確認 R-b1）。次の EndSession でも試さない。
void verify_end_session_tries_once()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.settings().fail(nenenib::application::SettingsFailure::unwritable);
    static_cast<void>(controller.apply(app::OpenSettings{}));
    focus_on(controller, core::EditorFeature::guide);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
    expect(editing.settings().writes() == 1,
           "EndSession over an open panel under a failure tries to write once");
    static_cast<void>(controller.apply(app::EndSession{app::SessionEnd::window_closed}));
    expect(editing.settings().writes() == 1, "a second EndSession does not try again");
}
} // namespace

void verify_settings_modal_scope()
{
    verify_end_session_tries_once();
    verify_open_change_close();
    verify_revert();
    verify_other_intents_close();
    verify_filter_and_sections();
    verify_presets();
    verify_immediate_look();
    verify_failure();
    verify_full_preview();
    verify_signals_and_session();
    verify_retry_at_session_end();
    verify_open_closes_input();
    verify_scrolls_keep_open();
    verify_faces_are_exclusive();
    verify_language_server_switch();
    verify_vim_wrap_window();
}
} // namespace nenenib::tests
