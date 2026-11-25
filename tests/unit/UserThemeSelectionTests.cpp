#include "ScriptedDocumentReads.hpp"
#include "ScriptedLanguageServer.hpp"
#include "ScriptedShellRegistration.hpp"
#include "ScriptedTextLayouts.hpp"
// scope `--user-theme-selection` の単体テスト（ADR 0042 決定 2）。
#include "Appearance.hpp"
#include "AsciiCase.hpp"
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
#include "ScriptedAutosave.hpp"
#include "ScriptedBell.hpp"
#include "ScriptedBookmarks.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedClock.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedFolders.hpp"
#include "ScriptedFonts.hpp"
#include "ScriptedHistory.hpp"
#include "ScriptedImages.hpp"
#include "ScriptedLanguageRoots.hpp"
#include "ScriptedLanguageServerSearch.hpp"
#include "ScriptedLinkOpen.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScriptedPreview.hpp"
#include "ScriptedPrinter.hpp"
#include "ScriptedProjectSearch.hpp"
#include "ScriptedReveal.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedStartLists.hpp"
#include "ScriptedThemes.hpp"
#include "ScriptedUiLanguage.hpp"
#include "SettingsFailure.hpp"
#include "SettingsIssue.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "ThemeCatalog.hpp"
#include "ThemeChoice.hpp"
#include "ThemeDerivation.hpp"
#include "ThemeDocument.hpp"
#include "ThemeExpectations.hpp"
#include "ThemeFailure.hpp"
#include "ThemeLookupFailure.hpp"
#include "ThemeName.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <functional>
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
    expect(names.size() == core::builtin_themes.size() + 3 &&
               std::ranges::is_sorted(names, core::ascii_folded_before) &&
               std::ranges::count(names, "broken") == 1 &&
               std::ranges::count(names, "my-theme") == 1 &&
               std::ranges::count(names, "z-theme") == 1,
           "user names merge with builtins in name order");
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
    auto palette = core::CommandPalette::opened({}, ":", core::EditMode::ordinary,
                                                core::CommandLine::empty(catalog))
                       .inserted("my-t")
                       .value();
    const auto found = palette.choice_at(0);
    expect(found.has_value() && found.value().command == "colorscheme my-theme",
           "palette finds the same user theme");
    palette = palette.filled("colorscheme broken").value();
    const auto broken_choice = palette.choice_at(0);
    expect(broken_choice.has_value() && broken_choice.value().command == "colorscheme broken",
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
    static_cast<void>(controller.apply_frame(InsertText{"body"}));
    const auto before = controller.frame();
    static_cast<void>(controller.apply_frame(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply_frame(app::CommandText{":my-t"}));
    auto frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.settings.theme == user_choice("my-theme") && frame.appearance == Appearance::light,
           "controller applies user theme and appearance");
    expect(editor.settings().writes() == 1 &&
               editor.settings().written().value_or(core::default_editor_settings()).theme ==
                   user_choice("my-theme"),
           "controller persists the resolved choice once");
    expect(frame.lines.front().text == "body" && frame.caret == before.caret,
           "theme leaves body and caret intact");
    static_cast<void>(controller.apply_frame(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply_frame(app::CommandText{":broken"}));
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
                   "broken: Invalid RGB or RGBA color" &&
               editor.settings().writes() == 1,
           "failed selection reports reason without writing settings");
    expect(frame.settings.theme == user_choice("my-theme"),
           "failed selection keeps previous appearance");
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
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

// 起動引数の文書 initial.txt と settings・themes で controller を作り、check に渡す（#1790 で
// 関数の長さの上限のため 2 つの起動の足場を 1 つにした）。
void with_startup(ScriptedSettings &settings, ScriptedThemes &themes,
                  const std::function<void(EditorController &)> &check)
{
    ScriptedAppearance appearance{Reading{Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedFiles files;
    files.hold(std::string("initial body"));
    ScriptedCodePages pages;
    ScriptedSession session;
    ScriptedHistory history;
    ScriptedFolders folders;
    ScriptedAutosave autosave;
    ScriptedFonts fonts;
    ScriptedBell bell;
    ScriptedClock wall_clock;
    ScriptedPrinter printer;
    ScriptedShellRegistration shell_registration;
    ScriptedImages images;
    ScriptedStartLists start_lists;
    ScriptedUiLanguage ui_language;
    ScriptedProjectSearch project_search;
    ScriptedPreview previews;
    ScriptedReveal reveal;
    ScriptedLinkOpen links;
    ScriptedLanguageRoots language_roots;
    ScriptedLanguageServerSearch searches;
    ScriptedMarkdown markdown;
    ScriptedBookmarks bookmarks;
    ScriptedTextLayouts text_layouts;
    ScriptedDocumentReads reads{files, pages};
    ScriptedLanguageServer language_server;
    EditorController controller{
        app::EditorPorts{
            appearance,   clipboard,       files,          pages,          settings,
            themes,       session,         history,        folders,        bookmarks,
            text_layouts, autosave,        fonts,          bell,           markdown,
            reads,        language_server, wall_clock,     printer,        shell_registration,
            images,       start_lists,     ui_language,    project_search, previews,
            reveal,       links,           language_roots, searches},
        nenenib::core::Distribution::unpackaged,
        {app::OpenDocument{FilePath::parse("initial.txt").value()}}};
    check(controller);
}

void verify_theme_startup_notice()
{
    ScriptedSettings settings;
    ScriptedThemes themes{user_catalog(), fixed_text("invalid_name.v1.theme: invalid name")};
    with_startup(settings, themes,
                 [&themes](EditorController &controller)
                 {
                     const auto started = controller.frame();
                     expect(started.lines.front().text == "initial body" &&
                                started.command_message.value_or(fixed_text("none")).text() ==
                                    "invalid_name.v1.theme: invalid name" &&
                                controller.apply_frame(VisibleLines{8}).command_message.has_value(),
                            "initial document uses normal file load and the startup diagnostic "
                            "survives it");
                     expect(!controller.apply_frame(InsertText{"x"}).command_message.has_value() &&
                                themes.reads() == 1,
                            "user input clears notice without rereading themes");
                 });
    ScriptedSettings broken{SettingsReading{std::unexpect, app::SettingsFailure::malformed}};
    with_startup(broken, themes,
                 [](EditorController &unreadable)
                 {
                     expect(unreadable.frame().lines.front().text == "initial body" &&
                                unreadable.frame().settings_failure ==
                                    app::SettingsFailure::malformed,
                            "failed settings do not prevent opening initial document");
                 });
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
    static_cast<void>(controller.apply_frame(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply_frame(app::CommandText{":colorscheme system"}));
    const auto frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.settings_failure == failure,
           "same-setting command cannot clear blocked startup error");
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
               "Settings could not be saved",
           "blocked no-op must not report a successful setting change");
    expect(editor.settings().writes() == 1 && !editor.settings().written().has_value(),
           "same value still consults the blocked settings port");
}

void verify_user_completion_order()
{
    const auto catalog = user_catalog();
    const auto themes = expected_theme_commands({"broken", "my-theme", "z-theme"});
    expect(core::command_completions("colorscheme ", catalog) == themes,
           "theme prefix merges builtins and custom names in name order");
    auto all = core::ex_command_candidates();
    all.resize(all.size() - core::builtin_themes.size() - 1);
    all.insert(all.end(), themes.begin(), themes.end());
    expect(core::ex_command_candidates(catalog) == all,
           "custom catalog extends the verified built-in catalog in exact order");
    expect(core::command_completions("colorscheme broken", catalog) ==
               std::vector<std::string>{"colorscheme broken"},
           "failed custom name remains a completion");
}

// 読めなかった理由ごとの文（docs/i18n/inventory-2026-11-13.tsv の theme_failure_* の 15
// 行・#2013）。
constexpr std::array<std::pair<core::ThemeFailure, std::string_view>, 15> theme_failure_texts{{
    {core::ThemeFailure::not_found, "Theme file not found"},
    {core::ThemeFailure::unreadable, "Theme file could not be read"},
    {core::ThemeFailure::too_large, "Theme file exceeds 16 KiB"},
    {core::ThemeFailure::malformed, "Expected key=value lines"},
    {core::ThemeFailure::duplicate_key, "Duplicate theme field"},
    {core::ThemeFailure::unknown_key, "Unknown theme field"},
    {core::ThemeFailure::missing_field, "Required theme field missing"},
    {core::ThemeFailure::unsupported_version, "Unsupported theme version"},
    {core::ThemeFailure::invalid_name, "Invalid theme filename or name"},
    {core::ThemeFailure::name_mismatch, "Theme name does not match filename"},
    {core::ThemeFailure::reserved_name, "Reserved theme name"},
    {core::ThemeFailure::invalid_text, "Invalid UTF-8 or theme attribution"},
    {core::ThemeFailure::invalid_appearance, "Appearance must be dark or light"},
    {core::ThemeFailure::invalid_color, "Invalid RGB or RGBA color"},
    {core::ThemeFailure::insufficient_contrast, "Text/background contrast must be at least 4.5"},
}};

// 理由の表の行の番号。理由を足すとこの switch がコンパイルで落ちる（-Wswitch-enum・CPP-002）ので、
// 表に行を足す（#2016 のレビュー 推奨 2）。
[[nodiscard]] consteval std::size_t theme_failure_row(core::ThemeFailure failure) noexcept
{
    using core::ThemeFailure;
    switch (failure)
    {
    case ThemeFailure::not_found:
        return 0;
    case ThemeFailure::unreadable:
        return 1;
    case ThemeFailure::too_large:
        return 2;
    case ThemeFailure::malformed:
        return 3;
    case ThemeFailure::duplicate_key:
        return 4;
    case ThemeFailure::unknown_key:
        return 5;
    case ThemeFailure::missing_field:
        return 6;
    case ThemeFailure::unsupported_version:
        return 7;
    case ThemeFailure::invalid_name:
        return 8;
    case ThemeFailure::name_mismatch:
        return 9;
    case ThemeFailure::reserved_name:
        return 10;
    case ThemeFailure::invalid_text:
        return 11;
    case ThemeFailure::invalid_appearance:
        return 12;
    case ThemeFailure::invalid_color:
        return 13;
    case ThemeFailure::insufficient_contrast:
        return 14;
    }
    std::unreachable();
}

[[nodiscard]] consteval bool theme_failure_rows_in_order() noexcept
{
    for (std::size_t index = 0; index < theme_failure_texts.size(); ++index)
    {
        if (theme_failure_row(theme_failure_texts.at(index).first) != index)
        {
            return false;
        }
    }
    return theme_failure_row(core::ThemeFailure::insufficient_contrast) + 1U ==
           theme_failure_texts.size();
}

static_assert(theme_failure_rows_in_order(), "the reason table has one row for every ThemeFailure");

// 理由の文は表のとおりで、知らせはテーマの名前を前に置く。比較は名前と理由の両方を見る。
void verify_theme_failure_texts()
{
    for (const auto &[failure, text] : theme_failure_texts)
    {
        expect(core::theme_failure_reason(failure) == text,
               "each theme failure has the inventory's reason text");
        expect(core::theme_failure_message(core::ThemeLookupFailure{user_name("ocean"), failure})
                       .text() == "ocean: " + std::string(text),
               "the theme failure message is the name, a colon and the reason");
    }
    const core::ThemeLookupFailure missing{user_name("ocean"), core::ThemeFailure::not_found};
    expect(missing == core::ThemeLookupFailure{user_name("ocean"), core::ThemeFailure::not_found},
           "the same name and reason are equal");
    expect(
        !(missing == core::ThemeLookupFailure{user_name("ocean"), core::ThemeFailure::malformed}),
        "a different reason is a different failure");
    expect(!(missing == core::ThemeLookupFailure{user_name("lake"), core::ThemeFailure::not_found}),
           "a different name is a different failure");
}
} // namespace

void verify_user_theme_selection()
{
    verify_theme_failure_texts();
    verify_theme_catalog();
    verify_user_theme_commands();
    verify_user_completion_order();
    verify_user_theme_controller();
    verify_theme_startup_notice();
    verify_blocked_theme_noop();
}
} // namespace nenenib::tests
