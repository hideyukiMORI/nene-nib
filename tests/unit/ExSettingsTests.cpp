// scope `--ex-settings` の単体テスト（ADR 0042 決定 2）。
#include "AdjustFontSize.hpp"
#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandLayout.hpp"
#include "CommandLine.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "FontSize.hpp"
#include "FontSizeAdjustment.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InputLineView.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "PasteCommand.hpp"
#include "RefreshAppearance.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedSettings.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "StatusBarLayout.hpp"
#include "TestSupport.hpp"
#include "ThemeChoice.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::RefreshAppearance;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::builtin_themes;
using nenenib::core::BuiltinTheme;
using nenenib::core::EditMode;
using nenenib::core::HistoryDirection;
using nenenib::core::status_bar_layout;
using nenenib::core::VimCharacter;
using nenenib::core::VimMode;
using nenenib::core::width_of;

namespace core = nenenib::core;
namespace app = nenenib::application;

void verify_font_sizes()
{
    using nenenib::core::adjusted_font_size;
    using nenenib::core::FontSize;
    using nenenib::core::FontSizeAdjustment;
    const auto initial = nenenib::core::default_font_size();
    expect(initial.points() == 13.5F, "the default font is 13.5 points");
    expect(nenenib::core::font_size_dips(initial) == 18.0F, "13.5 pt is 18 DIP, not 13.5 DIP");
    for (const float points : {8.0F, 13.25F, 40.0F})
    {
        expect(FontSize::from_points(points).has_value(), "valid point sizes are accepted");
    }
    for (const float points : {7.99F, 40.01F, -1.0F, std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN()})
    {
        expect(!FontSize::from_points(points).has_value(), "invalid point sizes are rejected");
    }
    expect(adjusted_font_size(initial, FontSizeAdjustment::increase, 1).points() == 14.5F,
           "increase means one point");
    expect(adjusted_font_size(initial, FontSizeAdjustment::decrease, 2).points() == 11.5F,
           "wheel steps use the same point adjustment");
    expect(adjusted_font_size(initial, FontSizeAdjustment::increase, 0).points() == 13.5F,
           "zero wheel steps do nothing");
    expect(adjusted_font_size(initial, FontSizeAdjustment::increase,
                              std::numeric_limits<std::size_t>::max())
                   .points() == 40.0F,
           "a huge count clamps without arithmetic overflow");
    expect(adjusted_font_size(initial, FontSizeAdjustment::decrease, 100).points() == 8.0F,
           "decrease stops at eight points");
    const auto largest = FontSize::from_points(40.0F).value();
    expect(adjusted_font_size(largest, FontSizeAdjustment::reset, 0).points() == 13.5F,
           "reset is independent of the count");
}

void verify_settings_loading()
{
    auto saved = nenenib::core::default_editor_settings();
    saved.font_size = nenenib::core::FontSize::from_points(21.25F).value();
    saved.font_family = fixed_text("Consolas");
    saved.theme = nenenib::core::ThemeChoice::from(BuiltinTheme::neutral_light);
    Editing editor{SettingsReading{saved}};
    const auto frame = editor.controller().frame();
    expect(frame.settings.font_size.points() == 21.25F &&
               frame.settings.font_family.text() == "Consolas",
           "saved font settings are restored");
    expect(frame.appearance == Appearance::light, "an explicit theme overrides the system");
    editor.appearance().script(Reading{Appearance::dark});
    const auto refreshed = editor.controller().apply_frame(RefreshAppearance{});
    expect(refreshed.appearance == Appearance::light, "system refresh preserves an explicit theme");
    expect(editor.settings().writes() == 0, "loading and system refresh never rewrite settings");
    const auto held_frame = frame;
    static_cast<void>(editor.controller().apply_frame(InsertText{"abc"}));
    expect(held_frame.settings.font_family.text() == "Consolas",
           "a retained frame owns its font name");
}

void verify_settings_adjustment()
{
    using nenenib::application::AdjustFontSize;
    using nenenib::core::FontSizeAdjustment;
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply_frame(InsertText{"abc"}));
    static_cast<void>(controller.apply_frame(SelectAll{}));
    const auto before = controller.frame();
    const auto larger = controller.apply_frame(AdjustFontSize{FontSizeAdjustment::increase, 1});
    expect(larger.settings.font_size.points() == 14.5F && editor.settings().writes() == 1,
           "a changed point size is persisted once");
    const auto &written = editor.settings().written();
    expect(written.has_value() && written.value().font_size.points() == 14.5F,
           "the persisted size equals the displayed size");
    expect(larger.lines.front().text == "abc" &&
               larger.caret.position.column == before.caret.position.column,
           "font adjustment preserves text and caret");
    expect(larger.lines.front().selection.presence == before.lines.front().selection.presence,
           "font adjustment preserves the selection");
    const auto reset = controller.apply_frame(AdjustFontSize{FontSizeAdjustment::reset, 1});
    expect(reset.settings.font_size.points() == 13.5F, "reset restores the default");
    static_cast<void>(controller.apply_frame(AdjustFontSize{FontSizeAdjustment::reset, 1}));
    expect(editor.settings().writes() == 2, "resetting the same size does not write again");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "settings changes do not consume text undo steps");
}

void verify_settings_failures()
{
    using nenenib::application::AdjustFontSize;
    using nenenib::application::SettingsFailure;
    using nenenib::core::FontSizeAdjustment;
    Editing unreadable{SettingsReading{std::unexpect, SettingsFailure::unsupported_version}};
    auto &controller = unreadable.controller();
    expect(controller.frame().settings_failure == SettingsFailure::unsupported_version,
           "a bad settings version is exposed in the startup frame");
    static_cast<void>(controller.apply_frame(VisibleLines{20}));
    expect(controller.frame().settings_failure == SettingsFailure::unsupported_version,
           "layout notifications cannot swallow the startup diagnostic");
    unreadable.settings().fail(SettingsFailure::unsupported_version);
    const auto refused = controller.apply_frame(AdjustFontSize{FontSizeAdjustment::increase, 1});
    expect(refused.settings.font_size.points() == 13.5F && refused.settings_failure.has_value(),
           "a failed write leaves the displayed setting unchanged");
    Editing editor;
    editor.settings().fail(SettingsFailure::unwritable);
    const auto failed =
        editor.controller().apply_frame(AdjustFontSize{FontSizeAdjustment::decrease, 1});
    expect(failed.settings_failure == SettingsFailure::unwritable,
           "write failure is a typed result");
    editor.settings().fail(std::nullopt);
    const auto retried =
        editor.controller().apply_frame(AdjustFontSize{FontSizeAdjustment::decrease, 1});
    expect(retried.settings.font_size.points() == 12.5F && !retried.settings_failure.has_value(),
           "a later successful save clears the diagnostic");
}

void verify_ex_evaluation()
{
    const auto settings = core::default_editor_settings();
    const auto query = core::evaluate_ex("colorscheme", settings, Appearance::dark).value();
    expect(!query.settings.has_value() &&
               query.message.text() == "colorscheme=ubuntu-aubergine (system)",
           "query resolves the system theme without saving");
    for (const auto &theme : core::builtin_themes)
    {
        const auto result =
            core::evaluate_ex("colorscheme " + std::string(theme.name), settings, Appearance::dark)
                .value();
        expect(result.settings.has_value(), "a theme command returns settings");
        const auto resolved = result.settings.value_or(settings);
        expect(core::selected_theme(resolved, Appearance::light).name == theme.name,
               "every built-in theme uses the shared table");
    }
    const auto font =
        core::evaluate_ex("set guifont=MS Gothic:h21.25", settings, Appearance::dark).value();
    expect(font.settings.has_value(), "guifont returns an atomic family and size change");
    const auto selected = font.settings.value_or(settings);
    expect(selected.font_family.text() == "MS Gothic" && selected.font_size.points() == 21.25F,
           "font names may contain spaces");
    const auto system =
        core::evaluate_ex(" colorscheme system ", selected, Appearance::light).value();
    expect(!system.settings.value_or(settings).theme.has_value() &&
               system.message.text() == "colorscheme=neutral-light (system)",
           "system follows the OS");
    for (const auto text : {"8", "40", "13.5", "2e1"})
    {
        expect(core::FontSize::parse(text).has_value(), "settings and Ex share numeric parsing");
    }
    for (const auto text : {"", "8px", "NaN", "inf", "7.99", "40.01", " 12", "12 "})
    {
        expect(!core::FontSize::parse(text), "invalid point text is rejected before storage");
    }
}

void verify_ex_rejections()
{
    const auto settings = core::default_editor_settings();
    for (const auto text :
         {"", "w!", "2colorscheme dracula", "1,2set fontsize=12", ":colorscheme", "set",
          "set number", "set fontsize=41", "set fontsize=12|q", "colorscheme DRACULA",
          "colorscheme missing", "set guifont=:h12", "set guifont=Consolas",
          "set guifont=Consolas:h12x", "set guifont=Consolas:hNaN", "colorscheme\n"})
    {
        const auto result = core::evaluate_ex(text, settings, Appearance::dark);
        expect(!result, "unsupported Ex must not partially execute");
        if (!result)
        {
            expect(!core::ex_failure_message(result.error()).text().empty(),
                   "failures explain themselves");
        }
    }
    expect(!core::evaluate_ex(std::string(257, 'x'), settings, Appearance::dark),
           "long commands are rejected");
    expect(!core::evaluate_ex("\xFF", settings, Appearance::dark), "invalid UTF-8 is rejected");
    expect(!core::evaluate_ex("set guifont=Consolas|q:h12", settings, Appearance::dark),
           "a pipe cannot be stored as part of the font name");
}

void verify_command_editing()
{
    using core::CommandEdit;
    auto line = core::CommandLine::empty().inserted("a界😀z").value();
    expect(line.caret().value == 9, "caret counts UTF-8 bytes");
    line = line.edited(CommandEdit::left).edited(CommandEdit::backspace);
    expect(line.text() == "a界z" && line.caret().value == 4, "backspace removes one code point");
    line = line.edited(CommandEdit::left).edited(CommandEdit::erase);
    expect(line.text() == "az" && line.caret().value == 1, "delete respects a multibyte boundary");
    line = line.edited(CommandEdit::home).edited(CommandEdit::backspace).inserted("前").value();
    expect(line.text() == "前az", "home and empty-range backspace retain text");
    line = line.edited(CommandEdit::end).edited(CommandEdit::right).edited(CommandEdit::erase);
    expect(line.caret().value == 5 && line.text() == "前az", "end movement and delete clamp");
    expect(!line.inserted("\n") && !line.inserted("\t") && !line.inserted("\xFF"),
           "input is one valid line");
    expect(line.inserted("").has_value(), "empty clipboard text is harmless");
    const auto full = core::CommandLine::empty().inserted(std::string(256, 'x')).value();
    expect(!full.inserted("y") && full.text().size() == 256,
           "rejected input does not alter the line");
}

void verify_command_completions()
{
    using core::CommandEdit;
    auto line = core::CommandLine::empty().inserted("colorscheme ").value();
    const auto choices = line.completions();
    expect(choices.size() == core::builtin_themes.size() + 1, "theme candidates include system");
    for (std::size_t index = 0; index < choices.size() + 1; ++index)
    {
        line = line.edited(CommandEdit::complete_next);
        expect(line.text() == choices[index % choices.size()],
               "Tab cycles using the original prefix");
    }
    line = line.edited(CommandEdit::complete_previous);
    expect(line.text() == "colorscheme system", "Shift Tab wraps backwards");
    line = line.edited(CommandEdit::home);
    expect(!line.completion_index().has_value(), "manual movement resets completion");
    const auto first = core::CommandLine::empty().edited(CommandEdit::complete_previous);
    // 名前だけの候補の末尾は文書の命令の最後の `saveas`（ADR 0067）。
    expect(first.text() == "saveas", "backwards completion starts at the last command");
    expect(core::command_completions("set f") == std::vector<std::string>{"set fontsize="},
           "option prefix");
    const auto unknown = core::CommandLine::empty().inserted("unknown").value();
    expect(unknown.edited(CommandEdit::complete_next).text() == "unknown",
           "missing candidates leave text");
    for (const auto dpi : {96U, 144U, 192U})
    {
        const auto status = core::status_bar_layout(800, 450, dpi);
        const auto layout = core::command_layout(status, dpi, 10);
        expect(layout.input.right < status.items.front().left && layout.visible_rows <= 6,
               "completion never covers the right status items");
        const auto tiny = core::command_layout(core::status_bar_layout(40, 10, dpi), dpi, 10);
        expect(core::width_of(tiny.input) == 0 && tiny.visible_rows == 0,
               "small windows do not invert rectangles");
    }
}

void verify_ex_controller()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply_frame(InsertText{"preserved"}));
    static_cast<void>(controller.apply_frame(app::SelectEditMode{EditMode::vim}));
    const auto before = controller.frame();
    const auto result = run_ex(controller, "set guifont=Consolas:h18.5");
    expect(result.settings.font_size.points() == 18.5F && editor.settings().writes() == 1,
           "Ex saves once");
    expect(result.settings.font_family.text() == "Consolas" &&
               result.lines.front().text == "preserved",
           "command text is not inserted into the document");
    expect(result.caret.position.column == before.caret.position.column &&
               result.document.save_state == before.document.save_state,
           "settings preserve caret and dirty state");
    expect(!result.command_line.has_value() && result.command_message.has_value(),
           "Enter closes with a result");
    static_cast<void>(run_ex(controller, "set guifont=Consolas:h18.5"));
    static_cast<void>(run_ex(controller, "colorscheme"));
    expect(editor.settings().writes() == 1, "same settings and queries do not write");
    const auto notified = controller.apply_frame(VisibleLines{12});
    expect(notified.command_message.has_value(), "layout notification preserves the result");
    const auto changed = run_ex(controller, "colorscheme neutral-light");
    expect(changed.appearance == Appearance::light, "Ex updates the palette immediately");
    const auto old_settings = changed.settings;
    editor.settings().fail(app::SettingsFailure::unwritable);
    const auto failed = run_ex(controller, "set fontsize=20");
    expect(core::same_settings(failed.settings, old_settings) &&
               failed.settings_failure.has_value(),
           "failed persistence leaves the appearance intact");
    expect(failed.command_message.has_value(), "save failure is visible in the command result");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "commands add no text undo entry");
}

void verify_guide_settings()
{
    expect(core::default_editor_settings().guide == core::GuideVisibility::shown,
           "guide is shown by default");
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply_frame(InsertText{"body"}));
    static_cast<void>(controller.apply_frame(app::SelectEditMode{EditMode::vim}));
    const auto before = controller.frame();
    const auto hidden = run_ex(controller, "set noguide");
    expect(hidden.settings.guide == core::GuideVisibility::hidden &&
               editor.settings().writes() == 1,
           "noguide persists hidden guide through shared settings path");
    expect(hidden.lines.front().text == "body" && hidden.caret == before.caret &&
               hidden.lines.front().selection == before.lines.front().selection,
           "guide setting preserves text selection and caret");
    static_cast<void>(run_ex(controller, "set noguide"));
    expect(editor.settings().writes() == 1, "same guide setting does not save");
    for (const auto command : {"set fontsize=18", "set guifont=Consolas:h19", "colorscheme dracula",
                               "colorscheme system"})
    {
        expect(run_ex(controller, command).settings.guide == core::GuideVisibility::hidden,
               "font and theme commands preserve guide");
    }
    expect(controller.apply_frame(app::AdjustFontSize{core::FontSizeAdjustment::increase, 1})
                   .settings.guide == core::GuideVisibility::hidden,
           "relative font size also preserves guide");
    editor.settings().fail(app::SettingsFailure::unwritable);
    expect(run_ex(controller, "set guide").settings.guide == core::GuideVisibility::hidden,
           "failed save keeps guide hidden");
    editor.settings().fail(std::nullopt);
    expect(run_ex(controller, "set guide").settings.guide == core::GuideVisibility::shown,
           "guide returns only after successful save");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "guide commands add no body undo entries");
    expect(core::command_completions("set g") ==
                   std::vector<std::string>{"set guifont=", "set guide"} &&
               core::command_completions("set nog") == std::vector<std::string>{"set noguide"},
           "guide commands are in shared Ex completion catalog");
    auto saved = core::default_editor_settings();
    saved.guide = core::GuideVisibility::hidden;
    Editing restored{SettingsReading{saved}};
    expect(restored.controller().frame().settings.guide == core::GuideVisibility::hidden &&
               restored.settings().writes() == 0,
           "loading hidden guide preserves it without saving");
}

void verify_ex_input_isolation()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply_frame(InsertText{"body"}));
    static_cast<void>(controller.apply_frame(app::SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply_frame(app::VimKeyPress{core::VimCharacter{U'y'}}));
    static_cast<void>(controller.apply_frame(app::VimKeyPress{core::VimCharacter{U'y'}}));
    const auto saved_register = controller.vim_state().unnamed_register.value().text;
    auto frame = controller.apply_frame(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(frame.command_line.has_value(), "NORMAL colon opens a separate line");
    frame = controller.apply_frame(app::EditCommand{core::CommandEdit::backspace});
    expect(!frame.command_line.has_value(), "empty backspace cancels");
    static_cast<void>(controller.apply_frame(app::VimKeyPress{core::VimCharacter{U':'}}));
    editor.clipboard().hold(std::string("set fontsize=19"));
    frame = controller.apply_frame(app::PasteCommand{});
    expect(frame.command_line.has_value(), "paste keeps the command active");
    expect(frame.command_line.value_or(core::InputLineView{}).text == "set fontsize=19",
           "clipboard goes to Ex");
    editor.clipboard().hold(std::string("\nBAD"));
    frame = controller.apply_frame(app::PasteCommand{});
    expect(frame.command_message.has_value() && frame.lines.front().text == "body",
           "multiline paste is rejected");
    frame = controller.apply_frame(app::CancelCommand{});
    expect(!frame.command_line.has_value() && editor.settings().writes() == 0,
           "cancel does not save");
    expect(controller.vim_state().unnamed_register.value().text == saved_register,
           "command input preserves the register");
    static_cast<void>(controller.apply_frame(app::VimKeyPress{core::VimCharacter{U'2'}}));
    frame = controller.apply_frame(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(!frame.command_line.has_value(),
           "unsupported counted Ex does not execute as an uncounted command");
    static_cast<void>(controller.apply_frame(app::VimKeyPress{core::VimCharacter{U'v'}}));
    frame = controller.apply_frame(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(!frame.command_line.has_value() && frame.vim_mode == core::VimMode::visual,
           "visual ranges are not interpreted");
}
// タブの命令の名前と省略形と引数（ADR 0057 の決定 4）。省略は Vim 9.1 の実測（probe の節 F）。
[[nodiscard]] bool tab_request_is(std::string_view text, core::ExTabVerb verb,
                                  std::optional<std::size_t> number)
{
    const auto result = core::evaluate_ex(text, core::default_editor_settings(), Appearance::dark);
    if (!result.has_value())
    {
        return false;
    }
    const auto &tab = result.value().tab;
    return tab.has_value() && tab.value().verb == verb && tab.value().number == number &&
           !result.value().settings.has_value() && !result.value().highlight.has_value() &&
           !result.value().incsearch.has_value();
}

[[nodiscard]] bool ex_fails_with(std::string_view text, core::ExFailure failure)
{
    const auto result = core::evaluate_ex(text, core::default_editor_settings(), Appearance::dark);
    return !result.has_value() && result.error() == core::ExEvaluationFailure{failure};
}

void verify_ex_tab_names()
{
    using core::ExTabVerb;
    using Row = std::pair<std::string_view, ExTabVerb>;
    const std::array<Row, 17> names{{{"tabn", ExTabVerb::next},
                                     {"tabne", ExTabVerb::next},
                                     {"tabnex", ExTabVerb::next},
                                     {"tabnext", ExTabVerb::next},
                                     {"tabp", ExTabVerb::previous},
                                     {"tabpr", ExTabVerb::previous},
                                     {"tabprev", ExTabVerb::previous},
                                     {"tabprevious", ExTabVerb::previous},
                                     {"tabN", ExTabVerb::previous},
                                     {"tabNe", ExTabVerb::previous},
                                     {"tabNext", ExTabVerb::previous},
                                     {"tabc", ExTabVerb::close},
                                     {"tabcl", ExTabVerb::close},
                                     {"tabclose", ExTabVerb::close},
                                     {"tabnew", ExTabVerb::open},
                                     {"tabs", ExTabVerb::list},
                                     {"  tabs  ", ExTabVerb::list}}};
    for (const auto &[text, verb] : names)
    {
        expect(tab_request_is(text, verb, std::nullopt),
               "a tab command name and its abbreviations name one verb");
    }
    for (const std::string_view text :
         {"tab", "tabe", "tabm", "tabf", "tabnexts", "tabnewx", "Tabnext", "tabpreviouss"})
    {
        expect(ex_fails_with(text, core::ExFailure::unknown_command),
               "names outside the table stay unknown commands");
    }
}

void verify_ex_tab_arguments()
{
    using core::ExTabVerb;
    expect(tab_request_is("tabnext 2", ExTabVerb::next, 2) &&
               tab_request_is("tabn 9", ExTabVerb::next, 9) &&
               tab_request_is("tabnext 0", ExTabVerb::next, 0) &&
               tab_request_is("tabprevious 4", ExTabVerb::previous, 4) &&
               tab_request_is("tabN 1", ExTabVerb::previous, 1),
           "tabnext and tabprevious take one decimal number and leave its range to the caller");
    for (const std::string_view text :
         {"tabnext +1", "tabnext -1", "tabnext $", "4tabnext", "2tabnew", "tabnext!", "tabclose!",
          "tabnew note.txt", "tabclose 2", "tabs 1", "tabnext 1 2", "tabnext x",
          "tabnext 99999999999999999999999"})
    {
        expect(ex_fails_with(text, core::ExFailure::unsupported_argument),
               "forms Vim accepts but Nib does not yet are unsupported");
    }
    const auto result =
        core::evaluate_ex("tabnext +1", core::default_editor_settings(), Appearance::dark);
    expect(!result.has_value() && core::ex_failure_message(result.error(), " tabnext +1 ").text() ==
                                      "Not supported: tabnext +1",
           "the unsupported message names the input without a Vim error number");
    expect(
        core::ex_failure_message(core::ExEvaluationFailure{core::ExFailure::unknown_command}, "tab")
                .text() == "Unknown command",
        "other failures keep their own message");
    expect(core::ex_failure_message(core::ExFailure::unsupported_argument).text() ==
               "Not supported",
           "the bare message has no input");
}
} // namespace

void verify_ex_settings()
{
    verify_font_sizes();
    verify_settings_loading();
    verify_settings_adjustment();
    verify_settings_failures();
    verify_ex_evaluation();
    verify_ex_rejections();
    verify_command_editing();
    verify_command_completions();
    verify_ex_controller();
    verify_guide_settings();
    verify_ex_input_isolation();
    verify_ex_tab_names();
    verify_ex_tab_arguments();
}
} // namespace nenenib::tests
