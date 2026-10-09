// Issue #280 / ADR 0064。Ex から共通の一覧への境界を確かめる。
#include "CancelCommand.hpp"
#include "CommandText.hpp"
#include "Editing.hpp"
#include "ExResult.hpp"
#include "FolderBatch.hpp"
#include "HistoryAction.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PaletteMarks.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "WorkCompleted.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

core::FilePath file_path(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

bool request_is(std::string_view text, core::PaletteScope scope, std::string_view query)
{
    const auto result = core::evaluate_ex(text, core::default_editor_settings(), Appearance::dark);
    if (!result.has_value())
    {
        return false;
    }
    const auto &palette = result.value().palette;
    return palette.has_value() && palette.value().scope == scope &&
           palette.value().query == query && !result.value().tab.has_value() &&
           !result.value().settings.has_value() && !result.value().highlight.has_value() &&
           !result.value().incsearch.has_value();
}

std::string input_text(const app::EditorFrame &frame)
{
    return frame.command_line.has_value() ? frame.command_line.value().text : "<no input>";
}

std::optional<std::size_t> listed_count(const app::EditorFrame &frame)
{
    if (!frame.command_palette.has_value())
    {
        return std::nullopt;
    }
    return frame.command_palette.value().total;
}

std::optional<std::size_t> listed_selection(const app::EditorFrame &frame)
{
    if (!frame.command_palette.has_value())
    {
        return std::nullopt;
    }
    return frame.command_palette.value().selected;
}

void verify_names_and_queries()
{
    for (const std::string_view name : {"e", "ed", "edi", "edit"})
    {
        expect(request_is(name, core::PaletteScope::files, ""), "edit aliases open all files");
        expect(request_is(std::string(name) + " 日本語 memo.txt", core::PaletteScope::files,
                          "日本語 memo.txt"),
               "edit arguments seed a literal search");
    }
    for (const std::string_view name :
         {"b", "bu", "buf", "buff", "buffe", "buffer", "ls", "files", "buffers"})
    {
        expect(request_is(name, core::PaletteScope::tabs, ""), "buffer aliases list tabs");
        expect(request_is(std::string(name) + " 2", core::PaletteScope::tabs, "2"),
               "a number is a search query rather than a direct switch");
    }
    expect(request_is("  edit  日本語  memo.txt  ", core::PaletteScope::files, "日本語  memo.txt"),
           "outer spaces are trimmed and inner spaces are kept");
    expect(core::command_completions("e") == std::vector<std::string>{"edit", "exit"} &&
               core::command_completions("b") == std::vector<std::string>{"buffer", "buffers"},
           "Ex completion exposes the full names from the common catalog");
}

void verify_rejections()
{
    for (const std::string_view input :
         {"e!", "edit! beta", "b! 2", "ls!", "2edit", "2b", "e +2 beta", "b#", "ls2", "e !x"})
    {
        const auto result =
            core::evaluate_ex(input, core::default_editor_settings(), Appearance::dark);
        expect(!result && result.error() ==
                              core::ExEvaluationFailure{core::ExFailure::unsupported_argument},
               "unimplemented Vim syntax does not become a file search");
    }
    for (const std::string_view input :
         {"file", "fi", "l", "lis", "edits", "bufferx", "Edit", "e | b"})
    {
        const auto result =
            core::evaluate_ex(input, core::default_editor_settings(), Appearance::dark);
        expect(!result &&
                   result.error() == core::ExEvaluationFailure{core::ExFailure::unknown_command},
               "distinct commands and command chaining are not accepted aliases");
    }
    for (const std::string_view input : {"e x\ny", "b \xFF"})
    {
        const auto result =
            core::evaluate_ex(input, core::default_editor_settings(), Appearance::dark);
        expect(!result &&
                   result.error() == core::ExEvaluationFailure{core::ExFailure::invalid_text},
               "file commands share the one-line UTF-8 boundary");
    }
}

void verify_palette_input()
{
    for (const auto &mark : core::palette_marks)
    {
        const auto input = core::palette_input_for(core::PaletteQuery{mark.scope, "日本語"});
        const auto parsed = core::palette_query_of(input);
        expect(parsed.scope == mark.scope && parsed.query == "日本語",
               "the shared mark table round-trips a scoped query");
        const std::string name = std::string(1, mark.mark) + "memo";
        const auto escaped =
            core::palette_input_for(core::PaletteQuery{core::PaletteScope::files, name});
        expect(escaped == " " + name &&
                   core::palette_query_of(escaped).scope == core::PaletteScope::files,
               "an initial mark in a file name remains a literal all-files query");
    }
    const std::string query(254, 'x');
    expect(request_is("b " + query, core::PaletteScope::tabs, query), "the longest command fits");
    const auto long_input =
        core::evaluate_ex("b " + query + 'x', core::default_editor_settings(), Appearance::dark);
    expect(!long_input &&
               long_input.error() == core::ExEvaluationFailure{core::ExFailure::too_long},
           "one more byte is rejected before creating a palette");
    Editing editing;
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    const auto frame = run_ex(editing.controller(), "b " + query);
    expect(frame.command_palette.has_value() && input_text(frame) == "#" + query,
           "adding the tabs mark to the largest accepted query still fits shared input");
}

void prepare_files(Editing &editing)
{
    editing.files().hold("alpha");
    applied(editing.controller(), app::OpenDocument{file_path("C:\\notes\\alpha.txt")});
    editing.files().hold("beta");
    applied(editing.controller(), app::OpenDocument{file_path("C:\\notes\\beta.txt")});
    editing.history().serve(app::FileHistory{{file_path("C:\\notes\\history.txt")}});
    editing.bookmarks().serve(app::FileBookmarks{{file_path("C:\\notes\\bookmark.txt")}});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
}

void verify_lists()
{
    Editing editing;
    prepare_files(editing);
    auto &controller = editing.controller();
    for (const std::string_view command : {"b", "ls", "files", "buffers"})
    {
        const auto frame = run_ex(controller, std::string(command));
        expect(frame.command_palette.has_value() && listed_count(frame) == 2 &&
                   listed_selection(frame) == 1 && input_text(frame) == "#",
               "tab aliases select the active tab and exclude closed files");
        expect(frame.ime == app::ImeStance::closed_once,
               "Ex palette shares the IME opening stance");
        applied(controller, app::CancelCommand{});
    }
    auto frame = run_ex(controller, "e");
    expect(listed_count(frame) == 4 && input_text(frame).empty(),
           "edit opens tabs plus bookmarks plus history");
    const auto ticket = editing.folders().requests().back().ticket;
    editing.folders().serve(app::FolderBatch{
        ticket, {file_path("C:\\notes\\folder.txt")}, app::FolderProgress::complete});
    frame = controller.apply_frame(app::WorkCompleted{});
    expect(listed_count(frame) == 5 && editing.files().reads() == 2,
           "the existing worker path appends folder candidates without opening them");
    applied(controller, app::CancelCommand{});
    frame = run_ex(controller, "b alpha");
    expect(listed_count(frame) == 1 && listed_selection(frame) == 0 &&
               input_text(frame) == "#alpha" && frame.active_tab == 1,
           "a buffer query filters before any switch");
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.active_tab == 0 && !frame.command_palette.has_value() &&
               editing.files().reads() == 2,
           "confirming an already open file switches without rereading");
}

void verify_open_and_preserve()
{
    Editing editing;
    prepare_files(editing);
    auto &controller = editing.controller();
    vim_replay(controller, "yyA!<Esc>");
    const auto saved_register = controller.vim_state().unnamed_register.text;
    const auto before = controller.frame();
    auto frame = run_ex(controller, "edit bookmark");
    expect(listed_count(frame) == 1 && frame.active_tab == 1 && vim_body(frame) == "beta!" &&
               editing.files().reads() == 2,
           "opening a query preserves the dirty document and defers file reading");
    editing.files().hold("marked");
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.tabs.size() == 3 && vim_body(frame) == "marked" && editing.files().reads() == 3,
           "confirming a closed file uses the existing open route");
    applied(controller, app::SwitchTab{1});
    frame = run_ex(controller, "e");
    frame = controller.apply_frame(app::CancelCommand{});
    expect(frame.caret == before.caret && frame.document.save_state == before.document.save_state &&
               vim_body(frame) == "beta!" &&
               controller.vim_state().unnamed_register.text == saved_register,
           "cancel restores document position and preserves dirty state and Vim register");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "beta",
           "the document undo history survives file selection");
    expect(editing.settings().writes() == 0 && editing.history().writes() == 0 &&
               editing.bookmarks().writes() == 0,
           "file commands write no settings or file catalogs");
}

void verify_literal_query_and_empty_result()
{
    Editing editing;
    prepare_files(editing);
    editing.bookmarks().serve(app::FileBookmarks{{file_path("C:\\notes\\#日本語 memo.txt")}});
    auto &controller = editing.controller();
    auto frame = run_ex(controller, "e #日本語 memo");
    expect(input_text(frame) == " #日本語 memo" && listed_count(frame) == 1,
           "Japanese and a leading scope mark match the literal bookmarked name");
    applied(controller, app::CancelCommand{});
    frame = run_ex(controller, "ls absent");
    expect(listed_count(frame) == 0, "an unmatched query shows an empty list");
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.command_palette.has_value() && frame.tabs.size() == 2 &&
               editing.files().reads() == 2,
           "Enter with no match opens no document");
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{":edit"});
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(frame.command_palette.has_value() && input_text(frame).empty(),
           "the command candidate also opens the same all-files palette");
}
} // namespace

void verify_ex_files_contracts()
{
    verify_names_and_queries();
    verify_rejections();
    verify_palette_input();
    verify_lists();
    verify_open_and_preserve();
    verify_literal_query_and_empty_result();
}

void verify_ex_files_scope()
{
    verify_ex_files_contracts();
}
} // namespace nenenib::tests
