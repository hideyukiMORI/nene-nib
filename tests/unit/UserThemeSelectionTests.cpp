// scope `--user-theme-selection` の単体テスト（ADR 0042 決定 2）。
#include "Appearance.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "CommandText.hpp"
#include "DisplayWidthRange.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorPorts.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "SettingsFailure.hpp"
#include "SettingsIssue.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeChoice.hpp"
#include "ThemeDerivation.hpp"
#include "ThemeDocument.hpp"
#include "ThemeFailure.hpp"
#include "ThemeLookupFailure.hpp"
#include "ThemeName.hpp"
#include "VisibleLines.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::OpenDocument;
using nenenib::application::VisibleLines;
using nenenib::core::builtin_themes;
using nenenib::core::BuiltinTheme;
using nenenib::core::HistoryDirection;
using nenenib::core::theme_of;

namespace core = nenenib::core;
namespace app = nenenib::application;

core::ThemeName user_name(std::string_view text)
{
    return core::ThemeName::parse(text).value();
}

core::ThemeChoice user_choice(std::string_view name)
{
    const auto &base = core::theme_of(core::BuiltinTheme::neutral_light);
    return core::ThemeChoice::from(
        core::ThemeDocument{user_name(name),
                            base.appearance,
                            base.ui,
                            base.body,
                            {fixed_text("作者"), fixed_text("MIT"), fixed_text("local")}});
}

core::ThemeCatalog user_catalog()
{
    return core::ThemeCatalog::from(
               {{user_name("z-theme"), user_choice("z-theme")},
                {user_name("broken"), std::unexpected(core::ThemeFailure::invalid_color)},
                {user_name("my-theme"), user_choice("my-theme")}})
        .value();
}

void verify_theme_catalog()
{
    auto catalog = user_catalog();
    const auto names = catalog.names();
    expect(names.size() == core::builtin_themes.size() + 3 && names.at(9) == "broken" &&
               names.at(10) == "my-theme" && names.at(11) == "z-theme",
           "user names follow builtins in sorted order");
    const auto choice = catalog.find(user_name("my_theme")).value();
    const auto copy = catalog;
    catalog = core::ThemeCatalog::builtins();
    expect(choice.view().source.author == "作者" && choice.name() == "my-theme",
           "choice keeps owned data after its catalog is replaced");
    expect(copy.find(user_name("my-theme")).value() == choice, "catalog copy shares live choices");
    expect(catalog.records().empty(), "builtin-only catalog has no user records");
    const auto alias = catalog.find(user_name("night_owl_light")).value();
    expect(alias.name() == "night-owl-light", "built-in aliases keep their canonical spelling");
    const auto missing = copy.find(user_name("missing"));
    const auto broken = copy.find(user_name("broken"));
    expect(!missing && missing.error() == core::ThemeLookupFailure{user_name("missing"),
                                                                   core::ThemeFailure::not_found},
           "missing theme names survive lookup failure");
    expect(!broken && broken.error().reason == core::ThemeFailure::invalid_color,
           "broken themes keep their actual load failure");
    expect(!core::ThemeCatalog::from({{user_name("system"), user_choice("system")}}),
           "system is reserved");
    expect(!core::ThemeCatalog::from({{user_name("dracula"), user_choice("dracula")}}),
           "builtin is reserved");
    expect(!core::ThemeCatalog::from({{user_name("a"), user_choice("b")}}),
           "record must match value name");
    expect(!core::ThemeCatalog::from(
               {{user_name("a"), user_choice("a")}, {user_name("a"), user_choice("a")}}),
           "duplicate names are rejected");
}

void verify_user_theme_commands()
{
    const auto catalog = user_catalog();
    const auto settings = core::default_editor_settings();
    const auto selected =
        core::evaluate_ex("colorscheme my_theme", settings, Appearance::dark, catalog).value();
    expect(selected.settings.value_or(settings).theme == user_choice("my-theme"),
           "Ex resolves user aliases canonically");
    const auto resolved = selected.settings.value_or(settings);
    expect(core::selected_theme(resolved, Appearance::dark).appearance == Appearance::light,
           "explicit user appearance wins over system");
    const auto broken =
        core::evaluate_ex("colorscheme broken", settings, Appearance::dark, catalog);
    expect(!broken && core::ex_failure_message(broken.error()).text() ==
                          "broken: Invalid RGB or RGBA color",
           "Ex explains named load failure");
    auto line = core::CommandLine::empty(catalog).inserted("colorscheme my").value();
    line = line.edited(core::CommandEdit::complete_next);
    expect(line.text() == "colorscheme my-theme", "Ex Tab completes user theme");
    line = line.edited(core::CommandEdit::backspace);
    expect(line.completions().front() == "colorscheme my-theme", "editing preserves catalog");
    auto palette = core::CommandPalette::opened(catalog).inserted("my-t").value();
    expect(palette.choices().front().command == "colorscheme my-theme",
           "palette finds the same user theme");
    palette = palette.filled("colorscheme broken").value();
    expect(palette.choices().front().command == "colorscheme broken",
           "failed theme remains actionable after fill");
    const auto system =
        core::evaluate_ex("colorscheme system", selected.settings.value_or(settings),
                          Appearance::dark, catalog)
            .value();
    expect(system.settings.has_value() && !system.settings.value().theme.has_value(),
           "system clears explicit user choice");
}

void verify_user_theme_controller()
{
    Editing editor{std::nullopt, user_catalog()};
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"body"}));
    const auto before = controller.frame();
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"my-t"}));
    auto frame = controller.apply(app::SubmitCommand{});
    expect(frame.settings.theme == user_choice("my-theme") && frame.appearance == Appearance::light,
           "controller applies user theme and appearance");
    expect(editor.settings().writes() == 1 &&
               editor.settings().written().value_or(core::default_editor_settings()).theme ==
                   user_choice("my-theme"),
           "controller persists the resolved choice once");
    expect(frame.lines.front().text == "body" && frame.caret == before.caret,
           "theme leaves body and caret intact");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"broken"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
                   "broken: Invalid RGB or RGBA color" &&
               editor.settings().writes() == 1,
           "failed selection reports reason without writing settings");
    expect(frame.settings.theme == user_choice("my-theme"),
           "failed selection keeps previous appearance");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "",
           "theme commands do not enter document history");
}

template <typename T>
concept CanBorrowName = requires(T &&value) { std::forward<T>(value).name(); };

template <typename T>
concept CanBorrowRecords = requires(T &&value) { std::forward<T>(value).records(); };

template <typename T>
concept CanSelectTheme =
    requires(T &&value) { core::selected_theme(std::forward<T>(value), Appearance::dark); };

static_assert(CanBorrowName<const core::ThemeChoice &> && !CanBorrowName<core::ThemeChoice>);

static_assert(CanBorrowRecords<const core::ThemeCatalog &> &&
              !CanBorrowRecords<core::ThemeCatalog>);

static_assert(CanSelectTheme<const core::EditorSettings &> &&
              !CanSelectTheme<core::EditorSettings>);

void verify_theme_startup_notice()
{
    ScriptedAppearance appearance{Reading{Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedFiles files;
    files.hold(std::string("initial body"));
    ScriptedCodePages pages;
    ScriptedSettings settings;
    ScriptedThemes themes{user_catalog(), fixed_text("invalid_name.v1.theme: invalid name")};
    const auto initial = app::OpenDocument{FilePath::parse("initial.txt").value()};
    EditorController controller{
        app::EditorPorts{appearance, clipboard, files, pages, settings, themes}, initial};
    auto frame = controller.frame();
    expect(frame.lines.front().text == "initial body", "initial document uses normal file load");
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
               "invalid_name.v1.theme: invalid name",
           "startup diagnostic survives initial file opening");
    frame = controller.apply(VisibleLines{8});
    expect(frame.command_message.has_value(), "first layout retains startup diagnostic");
    frame = controller.apply(InsertText{"x"});
    expect(!frame.command_message.has_value() && themes.reads() == 1,
           "user input clears notice without rereading themes");
    ScriptedSettings broken{SettingsReading{std::unexpect, app::SettingsFailure::malformed}};
    const EditorController unreadable{
        app::EditorPorts{appearance, clipboard, files, pages, broken, themes}, initial};
    expect(unreadable.frame().lines.front().text == "initial body" &&
               unreadable.frame().settings_failure == app::SettingsFailure::malformed,
           "failed settings do not prevent opening initial document");
}

template <typename T>
concept CanBorrowCatalog = requires(T &&value) { std::forward<T>(value).catalog(); };

static_assert(CanBorrowCatalog<const core::CommandLine &> && !CanBorrowCatalog<core::CommandLine>);

void verify_blocked_theme_noop()
{
    const app::SettingsIssue failure =
        core::ThemeLookupFailure{user_name("missing"), core::ThemeFailure::not_found};
    Editing editor{SettingsReading{std::unexpect, failure}};
    editor.settings().fail(failure);
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"colorscheme system"}));
    const auto frame = controller.apply(app::SubmitCommand{});
    expect(frame.settings_failure == failure,
           "same-setting command cannot clear blocked startup error");
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
               "Settings could not be saved",
           "blocked no-op must not report a successful setting change");
    expect(editor.settings().writes() == 1 && !editor.settings().written().has_value(),
           "same value still consults the blocked settings port");
}
} // namespace

void verify_user_theme_selection()
{
    verify_theme_catalog();
    verify_user_theme_commands();
    verify_user_theme_controller();
    verify_theme_startup_notice();
    verify_blocked_theme_noop();
}
} // namespace nenenib::tests
