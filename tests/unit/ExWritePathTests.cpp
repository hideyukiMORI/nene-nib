// Issue #286 / ADR 0067 / D39。名前付き保存の意味とFilePortへの要求。
#include "Editing.hpp"
#include "ExResult.hpp"
#include "HistoryAction.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

core::FilePath target_path()
{
    return core::FilePath::parse("C:\\other\\日本語 copy.txt").value();
}

void prepare(Editing &editing, bool named)
{
    applied(editing.controller(), app::VisibleLines{vim_visible_lines});
    if (named)
    {
        editing.files().hold(Bytes{"alpha\n"});
        applied(editing.controller(), app::OpenDocument{sample_path()});
    }
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    editing.files().resolve_to(target_path());
}

std::string message(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

bool path_is(std::string_view command, std::string_view wanted)
{
    const auto result =
        core::evaluate_ex(command, core::default_editor_settings(), core::Appearance::dark);
    if (!result)
    {
        return false;
    }
    const auto &document = result.value().document;
    if (!document.has_value())
    {
        return false;
    }
    const auto &path = document.value().path;
    return path.has_value() && path.value().text() == wanted;
}

void verify_syntax()
{
    for (const std::string_view name :
         {"w", "write", "wq", "x", "xit", "exit", "sav", "save", "savea", "saveas"})
    {
        expect(path_is(std::string(name) + " 日本語 copy.txt", "日本語 copy.txt"),
               "save command aliases accept a whole filename with spaces");
    }
    for (const std::string_view path :
         {"C:\\folder\\file.txt", "../日本語.txt", "\\\\host\\share\\a.txt"})
    {
        expect(path_is("w " + std::string(path), path),
               "parsing preserves Windows separators and relative names");
    }
    expect(path_is("w escaped\\ space.txt", "escaped space.txt"),
           "a backslash before a space escapes only that space");
    for (const std::string_view text :
         {"sav", "saveas", "sa file", "saveas! file", "q file", "q! file", "1w file", "%w file",
          "w ++enc=utf8 file", "w >>file", "w !cmd", "w *.txt", "w %", "w #", "w $HOME/file",
          "w ~/file", "w `cmd`", "w \"file\"", "w foo|q"})
    {
        expect(!core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark),
               "unsupported syntax cannot become a literal write target");
    }
}

void verify_copy_and_same_file()
{
    Editing editing;
    prepare(editing, true);
    auto &controller = editing.controller();
    vim_replay(controller, "yyA!<Esc>");
    const auto before = controller.frame();
    const auto held = controller.vim_state().unnamed_register.text;
    auto frame = run_ex(controller, "w copy.txt");
    expect(editing.files().written_path() == target_path().text() &&
               editing.files().written() == "alpha!\n" &&
               editing.files().write_mode() == app::FileWriteMode::create_new,
           "a copy uses the resolved target with create-only policy");
    expect(frame.document.path == before.document.path &&
               frame.document.save_state == core::SaveState::modified &&
               frame.caret == before.caret && controller.vim_state().unnamed_register.text == held,
           "copying retains the current identity, save point, caret and register");
    expect(message(run_ex(controller, "q")).starts_with("E37"),
           "the original document is still unsaved after a copy");
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    expect(vim_body(controller.frame()) == "alpha\n" &&
               controller.frame().document.save_state == core::SaveState::saved,
           "copying does not move the original save point");
    applied(controller, app::HistoryAction{core::HistoryDirection::redo});
    editing.files().resolve_to(sample_path());
    frame = run_ex(controller, "w current.txt");
    expect(frame.document.save_state == core::SaveState::saved &&
               editing.files().write_mode() == app::FileWriteMode::replace,
           "an explicit name resolving to the current file saves normally");
}

void verify_adopt()
{
    for (const bool named : {false, true})
    {
        Editing editing;
        prepare(editing, named);
        auto &controller = editing.controller();
        vim_replay(controller, "i!<Esc>");
        const auto frame = run_ex(controller, named ? "saveas next.txt" : "w next.txt");
        expect(frame.document.path == target_path() &&
                   frame.document.save_state == core::SaveState::saved &&
                   editing.files().write_mode() == app::FileWriteMode::create_new && !frame.closing,
               "successful saveas and untitled writes adopt the resolved filename");
        applied(controller, app::HistoryAction{core::HistoryDirection::undo});
        expect(controller.frame().document.save_state == core::SaveState::modified &&
                   controller.frame().document.path == target_path(),
               "undo after naming changes the body but keeps the new identity");
        applied(controller, app::HistoryAction{core::HistoryDirection::redo});
        expect(controller.frame().document.save_state == core::SaveState::saved,
               "redo reaches the new save point");
    }
}

void verify_failed_save(bool named, FileFailure failure)
{
    Editing editing;
    prepare(editing, named);
    auto &controller = editing.controller();
    vim_replay(controller, "i!<Esc>");
    const auto before = controller.frame();
    editing.files().refuse_writes(failure);
    const auto frame = run_ex(controller, named ? "sav next.txt" : "w next.txt");
    expect(frame.document.path == before.document.path &&
               frame.document.save_state == core::SaveState::modified &&
               vim_body(frame) == vim_body(before) && !frame.closing,
           "D39: a failed write retains the old name, body and unsaved state");
    expect(message(frame) == (failure == FileFailure::already_exists ? "E13: File exists"
                                                                     : "Could not write file") &&
               !frame.document.last_failure.has_value(),
           "Ex reports one inline failure");
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    expect(controller.frame().document.save_state == core::SaveState::saved,
           "failure keeps the previous save point");
}

void verify_failures()
{
    for (const bool named : {false, true})
    {
        for (const auto failure :
             {FileFailure::access_denied, FileFailure::unwritable, FileFailure::already_exists})
        {
            verify_failed_save(named, failure);
        }
    }
}

void verify_exit_conditions()
{
    for (const std::string_view command : {"wq", "x"})
    {
        Editing copy;
        prepare(copy, true);
        vim_replay(copy.controller(), "A!<Esc>");
        const auto refused = run_ex(copy.controller(), std::string(command) + " copy.txt");
        expect(!refused.closing && message(refused).starts_with("E37") &&
                   copy.files().written() == "alpha!\n",
               "writing a copy does not allow quitting the unsaved original");
        Editing unnamed;
        prepare(unnamed, false);
        vim_replay(unnamed.controller(), "i!<Esc>");
        const auto closed = run_ex(unnamed.controller(), std::string(command) + " named.txt");
        expect(closed.closing && closed.document.path == target_path() &&
                   unnamed.files().written() == "!",
               "naming an untitled document lets write-and-quit close it");
    }
    Editing clean;
    prepare(clean, true);
    clean.files().resolve_to(std::unexpected(FileFailure::unwritable));
    expect(run_ex(clean.controller(), "x output.txt").closing &&
               clean.files().resolve_input().empty() && clean.files().written_path().empty(),
           "unchanged xit skips resolution and writing");
}

void verify_resolution_and_other_tab()
{
    Editing failed;
    prepare(failed, true);
    failed.files().resolve_to(std::unexpected(FileFailure::unwritable));
    const auto frame = run_ex(failed.controller(), "saveas relative.txt");
    expect(frame.document.path == sample_path() &&
               failed.files().resolve_input() == "relative.txt" &&
               failed.files().written_path().empty() && message(frame) == "Could not write file",
           "resolution failure leaves the document untouched without writing");
    Editing tabs;
    prepare(tabs, true);
    auto &controller = tabs.controller();
    vim_replay(controller, "A!<Esc>");
    applied(controller, app::NewTab{});
    tabs.files().resolve_to(sample_path());
    const auto blocked = run_ex(controller, "sav same.txt");
    expect(message(blocked).starts_with("E139") && blocked.tabs.size() == 2 &&
               !blocked.document.path.has_value() && tabs.files().written_path().empty(),
           "an open tab cannot be overwritten by naming another document");
    applied(controller, app::SwitchTab{0});
    expect(vim_body(controller.frame()) == "alpha!\n", "the other tab retains its unsaved text");
}

void verify_encodings()
{
    Editing bom;
    bom.files().hold(Bytes{"\xEF\xBB\xBF日本語\r\n"});
    applied(bom.controller(), app::OpenDocument{sample_path()});
    prepare(bom, false);
    const auto copied = run_ex(bom.controller(), "w copy.txt");
    expect(bom.files().written() == "\xEF\xBB\xBF日本語\r\n" &&
               copied.document.path == sample_path(),
           "a filename copy preserves BOM and CRLF through the common encoder");

    Editing japanese;
    japanese.code_pages().decode_to(std::string("日"));
    japanese.code_pages().encode_to(std::string("\x93\xFA"));
    japanese.files().hold(Bytes{"\x93\xFA"});
    auto &controller = japanese.controller();
    applied(controller, app::OpenDocument{sample_path()});
    prepare(japanese, false);
    const auto renamed = run_ex(controller, "sav next.txt");
    expect(japanese.files().written() == "\x93\xFA" && renamed.document.path == target_path() &&
               renamed.document.encoding == core::TextEncoding::shift_jis,
           "saveas retains the document encoding when adopting a filename");
    vim_replay(controller, "A!<Esc>");
    japanese.files().resolve_to(sample_path());
    japanese.code_pages().encode_to(std::unexpected(CodePageFailure::unencodable));
    const auto failed = run_ex(controller, "sav unencodable.txt");
    expect(failed.document.path == target_path() &&
               failed.document.save_state == core::SaveState::modified &&
               japanese.files().written() == "\x93\xFA" &&
               message(failed).find("encoding") != std::string::npos,
           "D39: conversion failure keeps the previous name, save point and disk bytes");
}
} // namespace

void verify_ex_write_path_contracts()
{
    verify_syntax();
    verify_copy_and_same_file();
    verify_adopt();
    verify_failures();
    verify_exit_conditions();
    verify_resolution_and_other_tab();
    verify_encodings();
}
} // namespace nenenib::tests
