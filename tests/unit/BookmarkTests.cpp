// Issue #278 / ADR 0063。明示登録と Ctrl+P の境界だけを測る。
#include "BookmarkKey.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CommandText.hpp"
#include "ComposeText.hpp"
#include "EditCommand.hpp"
#include "Editing.hpp"
#include "EndSession.hpp"
#include "FileBookmarkEdit.hpp"
#include "FolderBatch.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PaletteOrigin.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SessionEnd.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "ToggleBookmark.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

app::FileBookmarks marked(std::initializer_list<std::string_view> names)
{
    app::FileBookmarks result;
    for (const auto name : names)
    {
        result.files.push_back(path_of(name));
    }
    return result;
}

std::string joined(const app::FileBookmarks &bookmarks)
{
    std::string text;
    for (const auto &path : bookmarks.files)
    {
        text += std::string(path.text()) + '|';
    }
    return text;
}

std::string stored(Editing &editing)
{
    return joined(editing.bookmarks().written().value_or(app::FileBookmarks{}));
}

std::string rows(const app::EditorFrame &frame)
{
    std::string text;
    if (!frame.command_palette.has_value())
    {
        return "<no palette>";
    }
    for (const auto &row : frame.command_palette.value().rows)
    {
        text += row.command + '|';
    }
    return text;
}

void open_file(Editing &editing, std::string_view path = "C:\\work\\a.txt")
{
    editing.files().hold_at(std::string(path), Bytes{"one two three"});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{path_of(path)});
}

app::EditorFrame palette(Editing &editing, std::string_view query)
{
    applied(editing.controller(), app::CancelCommand{});
    applied(editing.controller(), app::OpenCommandPalette{});
    return editing.controller().apply(app::CommandText{std::string(query)});
}

void verify_editing()
{
    ScriptedFiles files;
    const auto first = app::bookmarks_toggled({}, path_of("C:\\a.txt"), files);
    expect(first.has_value() && joined(first.value()) == "C:\\a.txt|",
           "the first file is appended");
    const auto next =
        app::bookmarks_toggled(marked({"C:\\a.txt", "C:\\b.txt"}), path_of("C:\\c.txt"), files);
    expect(joined(next.value()) == "C:\\a.txt|C:\\b.txt|C:\\c.txt|",
           "new files keep registration order");
    files.treat_as_same("C:\\A.txt", "C:\\a.txt");
    const auto removed = app::bookmarks_toggled(marked({"C:\\a.txt", "C:\\b.txt", "C:\\A.txt"}),
                                                path_of("C:\\A.txt"), files);
    expect(joined(removed.value()) == "C:\\b.txt|", "removing erases every spelling of that file");
    app::FileBookmarks full;
    for (std::size_t index = 0; index < app::bookmark_limit; ++index)
    {
        full.files.push_back(path_of(std::format("C:\\{}.txt", index)));
    }
    const auto refused = app::bookmarks_toggled(full, path_of("C:\\new.txt"), files);
    expect(!refused && refused.error() == app::FileBookmarksFailure::too_large &&
               app::bookmark_limit == 1024,
           "a full list refuses a new file without dropping old registrations");
    expect(app::bookmarks_toggled(full, path_of("C:\\42.txt"), files).value().files.size() == 1023,
           "removal remains possible at the limit");
}

void verify_keys()
{
    expect(core::toggles_bookmark(core::BookmarkKey::control_d, core::EditMode::ordinary),
           "ordinary Ctrl+D toggles");
    expect(!core::toggles_bookmark(core::BookmarkKey::control_shift_d, core::EditMode::ordinary),
           "ordinary shifted Ctrl+D is not assigned");
    expect(!core::toggles_bookmark(core::BookmarkKey::control_d, core::EditMode::vim),
           "Vim Ctrl+D remains a Vim key");
    expect(core::toggles_bookmark(core::BookmarkKey::control_shift_d, core::EditMode::vim),
           "Vim Ctrl+Shift+D toggles");
}

void verify_quiet_and_current()
{
    Editing editing;
    open_file(editing);
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"x"});
    applied(controller, app::SaveDocument{path_of("C:\\work\\a.txt"), core::TextEncoding::utf8});
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    expect(editing.bookmarks().reads() == 0 && editing.bookmarks().writes() == 0,
           "startup, open, text, save and tab switching perform no bookmark I/O");
    const auto before = controller.frame();
    const auto added = controller.apply(app::ToggleBookmark{});
    expect(stored(editing) == "C:\\work\\a.txt|" && editing.bookmarks().reads() == 1 &&
               editing.bookmarks().writes() == 1,
           "an explicit toggle reads then writes the current named file once");
    expect(vim_body(added) == vim_body(before) && added.active_tab == before.active_tab &&
               added.caret.position == before.caret.position &&
               added.document.save_state == before.document.save_state,
           "registration leaves text, caret, active tab and save state unchanged");
    editing.bookmarks().serve(marked({"C:\\work\\a.txt", "C:\\other.txt"}));
    applied(controller, app::ToggleBookmark{});
    expect(stored(editing) == "C:\\other.txt|",
           "the next toggle reads the latest list and preserves other windows' changes");
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    expect(editing.bookmarks().reads() == 2 && editing.bookmarks().writes() == 2,
           "closing a window never records bookmarks automatically");
}

void verify_palette_sources()
{
    Editing editing;
    open_file(editing);
    editing.files().treat_as_same("C:\\work\\A.TXT", "C:\\work\\a.txt");
    editing.bookmarks().serve(marked({"C:\\work\\b.txt", "C:\\work\\A.TXT", "C:\\work\\b.txt"}));
    editing.history().serve(
        app::FileHistory{{path_of("C:\\work\\b.txt"), path_of("C:\\work\\c.txt")}});
    auto frame = palette(editing, "");
    expect(rows(frame) == "C:\\work\\a.txt|C:\\work\\b.txt|C:\\work\\c.txt|",
           "tabs, bookmarks and history are ordered and unique");
    expect(frame.command_palette.has_value() &&
               frame.command_palette.value().rows.front().origin ==
                   core::PaletteOrigin::bookmarked_tab &&
               frame.command_palette.value().rows.at(1).origin == core::PaletteOrigin::bookmark,
           "an open bookmark has the combined origin while closed registrations are bookmarks");
    expect(rows(palette(editing, "*")) == "C:\\work\\a.txt|C:\\work\\b.txt|",
           "star includes open and closed bookmarks");
    expect(rows(palette(editing, "#")) == "C:\\work\\a.txt|",
           "hash includes the bookmarked tab once");
    expect(rows(palette(editing, "@")) == "C:\\work\\c.txt|", "history excludes bookmarks");
    applied(editing.controller(), app::CancelCommand{});
    applied(editing.controller(), app::OpenCommandPalette{});
    editing.folders().serve(
        app::FolderBatch{editing.folders().requests().back().ticket,
                         {path_of("C:\\work\\b.txt"), path_of("C:\\work\\d.txt")},
                         app::FolderProgress::complete});
    frame = editing.controller().apply(app::WorkCompleted{});
    expect(rows(frame) == "C:\\work\\a.txt|C:\\work\\b.txt|C:\\work\\c.txt|C:\\work\\d.txt|",
           "folder delivery excludes the registered file");
    const auto reads = editing.bookmarks().reads();
    applied(editing.controller(), app::CommandText{"*b"});
    applied(editing.controller(), app::EditCommand{core::CommandEdit::complete_next});
    expect(editing.bookmarks().reads() == reads && editing.bookmarks().writes() == 0,
           "filtering, selection and folder delivery never reread or write bookmarks");
}

void verify_selected_toggle()
{
    Editing editing;
    open_file(editing);
    editing.history().serve(app::FileHistory{{path_of("C:\\other\\b.txt")}});
    static_cast<void>(palette(editing, "@"));
    const auto reads = editing.files().reads();
    auto frame = editing.controller().apply(app::ToggleBookmark{});
    expect(stored(editing) == "C:\\other\\b.txt|" && !frame.command_palette.has_value() &&
               frame.active_tab == 0 && frame.tabs.size() == 1 && editing.files().reads() == reads,
           "toggling a selected history file registers it without opening or switching");
    static_cast<void>(palette(editing, "*"));
    frame = editing.controller().apply(app::ToggleBookmark{});
    expect(stored(editing).empty() && !frame.command_palette.has_value(),
           "the same selection command removes a bookmark");
    expect(rows(palette(editing, "*")).empty(), "reopening refreshes the removed list");
}

void verify_failures()
{
    Editing editing;
    open_file(editing);
    editing.bookmarks().serve(std::unexpected(app::FileBookmarksFailure::malformed));
    auto frame = editing.controller().apply(app::ToggleBookmark{});
    expect(editing.bookmarks().writes() == 0 && frame.command_message.has_value(),
           "a read failure warns and never overwrites the file");
    frame = editing.controller().apply(app::OpenCommandPalette{});
    expect(rows(frame) == "C:\\work\\a.txt|" && frame.command_message.has_value(),
           "a broken list still allows other candidates with a warning");
    editing.bookmarks().serve(marked({"C:\\other.txt"}));
    editing.bookmarks().fail(app::FileBookmarksFailure::unwritable);
    static_cast<void>(palette(editing, "*oth"));
    frame = editing.controller().apply(app::ToggleBookmark{});
    expect(frame.command_palette.has_value() &&
               frame.command_line.value_or(core::InputLineView{}).text == "*oth" &&
               rows(frame) == "C:\\other.txt|" && frame.command_palette.value().selected == 0 &&
               frame.command_message.has_value(),
           "a failed write preserves the input, selection, candidate and warning");
    expect(!editing.bookmarks().written().has_value(), "a failed write preserves storage");
}

void verify_missing_and_open()
{
    Editing editing;
    open_file(editing);
    editing.bookmarks().serve(marked({"C:\\gone.txt", "C:\\work\\a.txt"}));
    static_cast<void>(palette(editing, "*gone"));
    auto frame = editing.controller().apply(app::SubmitCommand{});
    expect(frame.command_message.has_value() && editing.bookmarks().writes() == 0 &&
               frame.tabs.size() == 1,
           "a missing file warns without removing its registration");
    expect(rows(palette(editing, "*gone")) == "C:\\gone.txt|",
           "the missing bookmark remains in the list");
    applied(editing.controller(), app::ToggleBookmark{});
    expect(stored(editing) == "C:\\work\\a.txt|",
           "a missing bookmark can be removed without opening it");
    applied(editing.controller(), app::CancelCommand{});
    open_file(editing, "C:\\work\\b.txt");
    const auto reads = editing.files().reads();
    static_cast<void>(palette(editing, "*"));
    frame = editing.controller().apply(app::SubmitCommand{});
    expect(frame.active_tab == 0 && frame.tabs.size() == 2 && editing.files().reads() == reads,
           "opening a bookmarked tab switches to it without a read or duplicate tab");
}

void verify_ignored_inputs()
{
    Editing editing;
    auto &controller = editing.controller();
    auto frame = controller.apply(app::ToggleBookmark{});
    expect(frame.command_message.has_value() && editing.bookmarks().reads() == 0,
           "an untitled document asks for a named file without I/O");
    open_file(editing);
    static_cast<void>(palette(editing, ":set"));
    const auto reads = editing.bookmarks().reads();
    frame = controller.apply(app::ToggleBookmark{});
    expect(frame.command_palette.has_value() &&
               frame.command_line.value_or(core::InputLineView{}).text == ":set" &&
               !frame.command_message.has_value() && editing.bookmarks().reads() == reads,
           "settings candidates ignore the shortcut");
    applied(controller, app::CancelCommand{});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    for (const auto key : {":", "/"})
    {
        vim_replay(controller, key);
        const auto before = controller.frame().command_line.value_or(core::InputLineView{}).text;
        frame = controller.apply(app::ToggleBookmark{});
        expect(frame.command_line.value_or(core::InputLineView{}).text == before &&
                   editing.bookmarks().reads() == reads,
               "Ex and search input ignore the shortcut");
        applied(controller, app::CancelCommand{});
    }
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    applied(controller, app::ComposeText{composed_of("あ", {}, 0)});
    frame = controller.apply(app::ToggleBookmark{});
    expect(frame.composition.has_value() && editing.bookmarks().reads() == reads &&
               editing.bookmarks().writes() == 0,
           "IME composition ignores the shortcut");
    applied(controller, app::CancelComposition{});
    static_cast<void>(palette(editing, "*"));
    const auto palette_reads = editing.bookmarks().reads();
    applied(controller, app::ComposeText{composed_of("あ", {}, 0)});
    frame = controller.apply(app::ToggleBookmark{});
    expect(frame.command_composition.has_value() && editing.bookmarks().reads() == palette_reads &&
               editing.bookmarks().writes() == 0,
           "palette composition ignores the shortcut too");
}

// ADR 0078: 操作の一覧（?）はファイルの面ではないので、設定のコマンドと同じく何もしない。
void verify_operation_list_ignored()
{
    Editing editing;
    open_file(editing);
    static_cast<void>(palette(editing, "?"));
    const auto reads = editing.bookmarks().reads();
    const auto frame = editing.controller().apply(app::ToggleBookmark{});
    expect(frame.command_palette.has_value() &&
               frame.command_line.value_or(core::InputLineView{}).text == "?" &&
               !frame.command_message.has_value() && editing.bookmarks().reads() == reads &&
               editing.bookmarks().writes() == 0,
           "the operation list ignores the shortcut like settings candidates");
}

void verify_vim_pending()
{
    Editing editing;
    open_file(editing);
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_replay(controller, "d");
    applied(controller, app::ToggleBookmark{});
    vim_replay(controller, "w");
    expect(vim_body(controller.frame()) == "two three",
           "registration preserves a pending Vim operator");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "one two three", "registration creates no undo entry");
}
} // namespace

void verify_bookmarks_contracts()
{
    verify_editing();
    verify_keys();
    verify_quiet_and_current();
    verify_palette_sources();
    verify_selected_toggle();
    verify_failures();
    verify_missing_and_open();
    verify_ignored_inputs();
    verify_operation_list_ignored();
    verify_vim_pending();
}

void verify_bookmarks_scope()
{
    verify_bookmarks_contracts();
}
} // namespace nenenib::tests
