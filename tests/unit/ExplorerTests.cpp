// Issue #997（ADR 0162 の決定 3〜6・9）: エクスプローラーの application の契約 `--explorer`。
// 意図 4 つと焦点の 3 状態、FolderPort の entries の往復と古い券、読めないフォルダの 1 行、
// ファイルを開く open_listed の道、設定の保存、Ctrl+P の面と port を分け合う順番を見る。
// port はどれも替え玉で、時刻・スレッド・OS には触れない。
#include "CommandInput.hpp"
#include "CommandLine.hpp"
#include "CommandText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExplorerKey.hpp"
#include "ExplorerKeyKind.hpp"
#include "ExplorerPointing.hpp"
#include "ExplorerRootChosen.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerTree.hpp"
#include "ExplorerView.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderListingKind.hpp"
#include "FolderProgress.hpp"
#include "ImeStance.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PointExplorer.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedFolders.hpp"
#include "ScriptedSettings.hpp"
#include "ScrollExplorer.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "TestSupport.hpp"
#include "ToggleExplorer.hpp"
#include "VimMode.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::ExplorerKey;
using app::ExplorerKeyKind;
using app::ExplorerPointing;
using app::FolderProgress;
using app::PointExplorer;

constexpr std::string_view project = "C:\\proj";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] SettingsReading shown_at(std::optional<std::string_view> root)
{
    auto settings = core::default_editor_settings();
    settings.explorer = true;
    if (root.has_value())
    {
        settings.root = path_of(root.value());
    }
    return settings;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("-");
}

[[nodiscard]] core::EditorSettings saved(Editing &editing)
{
    return editing.settings().written().value_or(core::default_editor_settings());
}

[[nodiscard]] app::ExplorerView view_of(const app::EditorFrame &frame)
{
    return frame.explorer.value_or(app::ExplorerView{});
}

[[nodiscard]] std::string printed_row(const app::ExplorerRowView &row)
{
    std::string folder;
    if (row.folder)
    {
        folder = row.open ? "/-" : "/+";
    }
    return std::format("{}{}{}{}{};", std::string(row.depth, '.'), row.more ? "..." : "", row.name,
                       folder, row.hidden ? "~" : "");
}

// 面の行（名前・段・フォルダ・開いている・隠し・「…」）と選択・先頭・焦点。面が無ければ "none"。
[[nodiscard]] std::string printed(const app::EditorFrame &frame)
{
    if (!frame.explorer.has_value())
    {
        return "none";
    }
    const auto view = view_of(frame);
    std::string text;
    for (const auto &row : view.rows)
    {
        text += printed_row(row);
    }
    const std::string selected =
        view.selected.has_value() ? std::to_string(view.selected.value()) : std::string("-");
    return text + std::format("sel {} first {} focus {}", selected, view.first_visible,
                              view.focused ? 1 : 0);
}

[[nodiscard]] app::FolderEntry entry_of(std::string_view path, bool folder, bool hidden = false)
{
    return app::FolderEntry{path_of(path), folder, hidden};
}

// 最後の要求（entries）の券で batch を仕込み、合図を送る。
app::EditorFrame deliver(Editing &editing, std::initializer_list<app::FolderEntry> entries,
                         FolderProgress progress)
{
    app::FolderBatch batch{editing.folders().requests().back().ticket, {}, progress};
    batch.entries.assign(entries.begin(), entries.end());
    editing.folders().serve(std::move(batch));
    return editing.controller().apply_frame(app::WorkCompleted{});
}

// C:\proj に .git（隠し）・src・README.md を読んだ面。
app::EditorFrame loaded_project(Editing &editing)
{
    applied(editing.controller(), app::VisibleLines{10});
    return deliver(editing,
                   {entry_of("C:\\proj\\src", true), entry_of("C:\\proj\\README.md", false),
                    entry_of("C:\\proj\\.git", true, true)},
                   FolderProgress::complete);
}

app::EditorFrame key(Editing &editing, ExplorerKeyKind kind)
{
    return editing.controller().apply_frame(ExplorerKey{kind});
}

void verify_toggle_focus()
{
    Editing editing;
    auto &controller = editing.controller();
    expect(printed(controller.frame()) == "none" && editing.folders().requests().empty(),
           "the panel is hidden by default and nothing is listed");
    const auto shown = controller.apply_frame(app::ToggleExplorer{});
    expect(printed(shown) == "sel - first 0 focus 1" && !view_of(shown).root.has_value() &&
               editing.settings().writes() == 1 && saved(editing).explorer,
           "a hidden panel is shown with the focus and the setting is saved");
    expect(editing.folders().requests().empty(), "a panel without a root lists nothing");
    const auto hidden = controller.apply_frame(app::ToggleExplorer{});
    expect(printed(hidden) == "none" && editing.settings().writes() == 2 &&
               !saved(editing).explorer,
           "a focused panel is hidden and the setting is saved");
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    const auto left = key(editing, ExplorerKeyKind::esc);
    expect(printed(left) == "sel - first 0 focus 0", "Esc only gives the focus back");
    const auto focused = controller.apply_frame(app::ToggleExplorer{});
    expect(printed(focused) == "sel - first 0 focus 1" && editing.settings().writes() == 3,
           "a shown panel without the focus only takes the focus and saves nothing");
    editing.settings().fail(app::SettingsFailure::unwritable);
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    expect(printed(controller.frame()) == "sel - first 0 focus 1",
           "a panel whose setting cannot be written stays as it was");
}

void verify_entries_round_trip()
{
    Editing editing(shown_at(project));
    const auto &requests = editing.folders().requests();
    expect(requests.size() == 1 && requests.back().folder == path_of(project) &&
               requests.back().listing == app::FolderListingKind::entries,
           "a shown panel lists the root's entries once at the start");
    applied(editing.controller(), app::VisibleLines{10});
    const auto partial = deliver(editing, {entry_of("C:\\proj\\src", true)}, FolderProgress::more);
    expect(printed(partial) == "sel - first 0 focus 0",
           "entries are placed only when the last batch arrives");
    const auto frame = deliver(
        editing, {entry_of("C:\\proj\\README.md", false), entry_of("C:\\proj\\.git", true, true)},
        FolderProgress::complete);
    expect(printed(frame) == ".git/+~;src/+;README.md;sel - first 0 focus 0",
           "folders come first, names are folded, hidden entries are marked");
    expect(view_of(frame).root == std::optional{path_of(project)}, "the frame carries the root");
    expect(editing.folders().requests().size() == 1, "a loaded root is not listed again");
}

void verify_keys()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    expect(printed(key(editing, ExplorerKeyKind::j)) ==
               ".git/+~;src/+;README.md;sel 0 first 0 focus 0",
           "j selects the first row when nothing is selected");
    static_cast<void>(key(editing, ExplorerKeyKind::down));
    const auto opened = key(editing, ExplorerKeyKind::l);
    expect(printed(opened) == ".git/+~;src/-;README.md;sel 1 first 0 focus 0" &&
               editing.folders().requests().back().folder == path_of("C:\\proj\\src") &&
               editing.folders().entry_requests() == 2,
           "l opens the folder and lists it in the background");
    static_cast<void>(key(editing, ExplorerKeyKind::right));
    expect(editing.folders().entry_requests() == 2, "an open folder is not listed again");
    const auto children =
        deliver(editing, {entry_of("C:\\proj\\src\\main.cpp", false)}, FolderProgress::complete);
    expect(printed(children) == ".git/+~;src/-;.main.cpp;README.md;sel 1 first 0 focus 0",
           "the children are placed under the folder and the selection stays on it");
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    const auto parent = key(editing, ExplorerKeyKind::h);
    expect(printed(parent) == ".git/+~;src/-;.main.cpp;README.md;sel 1 first 0 focus 0",
           "h on a file goes to the parent folder");
    const auto closed = key(editing, ExplorerKeyKind::left);
    expect(printed(closed) == ".git/+~;src/+;README.md;sel 1 first 0 focus 0",
           "h on an open folder closes it");
    static_cast<void>(key(editing, ExplorerKeyKind::up));
    expect(printed(key(editing, ExplorerKeyKind::k)) ==
               ".git/+~;src/+;README.md;sel 0 first 0 focus 0",
           "k stops at the first row");
    const auto reopened = key(editing, ExplorerKeyKind::enter);
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    expect(printed(reopened) == ".git/-~;src/+;README.md;sel 0 first 0 focus 0" &&
               editing.folders().entry_requests() == 3,
           "Enter on a folder opens it like l");
}

void verify_enter_opens_file()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    editing.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    static_cast<void>(editing.controller().apply_frame(app::ToggleExplorer{}));
    static_cast<void>(
        editing.controller().apply_frame(PointExplorer{2, ExplorerPointing::pressed}));
    static_cast<void>(
        editing.controller().apply_frame(PointExplorer{2, ExplorerPointing::released}));
    const auto reads = editing.reads().requests().size();
    const auto opened = key(editing, ExplorerKeyKind::enter);
    expect(opened.document.title.text() == "README.md" && !opened.lines.empty() &&
               opened.lines.front().text == "read me" &&
               editing.reads().requests().size() == reads + 1,
           "Enter on a file opens it through the read port");
    expect(printed(opened) == ".git/+~;src/+;README.md;sel 2 first 0 focus 0",
           "opening a file gives the focus back to the text and keeps the panel");
    expect(editing.history().reads() == 0 && editing.history().writes() == 0,
           "opening from the panel never touches the history");
    const auto failed =
        editing.controller().apply_frame(PointExplorer{2, ExplorerPointing::double_click});
    expect(message_of(failed) == "-" && failed.document.title.text() == "README.md",
           "double-clicking the opened file switches to its tab");
    Editing missing(shown_at(project));
    static_cast<void>(loaded_project(missing));
    const auto notice =
        missing.controller().apply_frame(PointExplorer{2, ExplorerPointing::double_click});
    expect(message_of(notice) == "開けませんでした: README.md" && missing.history().reads() == 0 &&
               missing.history().writes() == 0,
           "a file that cannot be opened leaves one notice line and the history alone");
}

void verify_pointing()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(PointExplorer{1, ExplorerPointing::pressed}));
    const auto moved = controller.apply_frame(PointExplorer{2, ExplorerPointing::released});
    expect(printed(moved) == ".git/+~;src/+;README.md;sel - first 0 focus 1",
           "pressing takes the focus and releasing on another row does nothing");
    static_cast<void>(controller.apply_frame(PointExplorer{1, ExplorerPointing::pressed}));
    const auto clicked = controller.apply_frame(PointExplorer{1, ExplorerPointing::released});
    expect(printed(clicked) == ".git/+~;src/-;README.md;sel 1 first 0 focus 1" &&
               editing.folders().entry_requests() == 2,
           "a click on a folder selects and opens it");
    static_cast<void>(controller.apply_frame(PointExplorer{1, ExplorerPointing::pressed}));
    const auto again = controller.apply_frame(PointExplorer{1, ExplorerPointing::released});
    expect(printed(again) == ".git/+~;src/+;README.md;sel 1 first 0 focus 1",
           "a second click closes it");
    const auto twice = controller.apply_frame(PointExplorer{1, ExplorerPointing::double_click});
    const auto after = controller.apply_frame(PointExplorer{1, ExplorerPointing::released});
    expect(printed(twice) == ".git/+~;src/-;README.md;sel 1 first 0 focus 1" &&
               printed(after) == printed(twice),
           "a double click opens the folder and its release does nothing");
    const auto outside = controller.apply_frame(PointExplorer{9, ExplorerPointing::double_click});
    expect(printed(outside) == printed(twice), "a row outside the rows does nothing");
}

// 押してから離すまでに子が届いて行がずれても、押した節と離した節が違えば何もしない（ADR 0056 の
// 決定 9 の形・レビュー R5-2）。
void verify_pointing_across_arrival()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    static_cast<void>(key(editing, ExplorerKeyKind::l));
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(PointExplorer{2, ExplorerPointing::pressed}));
    static_cast<void>(
        deliver(editing, {entry_of("C:\\proj\\src\\main.cpp", false)}, FolderProgress::complete));
    const auto shifted = controller.apply_frame(PointExplorer{2, ExplorerPointing::released});
    expect(printed(shifted) == ".git/+~;src/-;.main.cpp;README.md;sel 1 first 0 focus 1",
           "a release on the row a new child moved under the pointer does nothing");
    static_cast<void>(controller.apply_frame(PointExplorer{3, ExplorerPointing::pressed}));
    const auto same = controller.apply_frame(PointExplorer{3, ExplorerPointing::released});
    expect(printed(same) == ".git/+~;src/-;.main.cpp;README.md;sel 3 first 0 focus 1",
           "a press and release on the same entry selects it");
}

void verify_scroll()
{
    Editing editing(shown_at(project));
    applied(editing.controller(), app::VisibleLines{4});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, FolderProgress::complete};
    for (std::size_t index = 0; index < 10; ++index)
    {
        batch.entries.push_back(entry_of(std::format("C:\\proj\\f{}.txt", index), false));
    }
    editing.folders().serve(std::move(batch));
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    const auto down = controller.apply_frame(app::ScrollExplorer{3});
    expect(view_of(down).first_visible == 3 && !view_of(down).selected.has_value(),
           "the wheel moves the first row and not the selection");
    // 面の行数は本文の 4 行 − ルート名の 1 行 = 3（E4 の追補）。
    expect(view_of(down).total == 10 && view_of(down).rows.size() == 3 &&
               view_of(down).rows.front().name == "f3.txt" &&
               view_of(down).rows.back().name == "f5.txt",
           "the frame carries the panel's rows from the first visible row");
    expect(view_of(controller.apply_frame(app::ScrollExplorer{100})).first_visible == 7,
           "the wheel stops when the last row is at the bottom");
    expect(view_of(controller.apply_frame(app::ScrollExplorer{-100})).first_visible == 0,
           "the wheel stops at the first row");
    for (std::size_t step = 0; step < 6; ++step)
    {
        static_cast<void>(key(editing, ExplorerKeyKind::j));
    }
    const auto followed = controller.frame();
    expect(view_of(followed).selected == std::optional<std::size_t>{5} &&
               view_of(followed).first_visible == 3,
           "moving the selection keeps it in view");
}

// 面の行数は本文の見えている行数 − 1（ルート名の行・最小 1）を application の 1 か所で導き、
// 選んだ行は面の最後の行より下へ出ない（E4 の追補・ADR 0162 の決定 2）。
void verify_panel_rows()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{5});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, FolderProgress::complete};
    for (std::size_t index = 0; index < 12; ++index)
    {
        batch.entries.push_back(entry_of(std::format("C:\\proj\\g{:02}.txt", index), false));
    }
    editing.folders().serve(std::move(batch));
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    bool inside = true;
    for (std::size_t step = 0; step < 12; ++step)
    {
        const auto view = view_of(key(editing, ExplorerKeyKind::j));
        inside = inside && view.rows.size() == 4 && view.selected.has_value() &&
                 view.selected.value() >= view.first_visible &&
                 view.selected.value() < view.first_visible + view.rows.size();
    }
    expect(inside, "the selected row stays inside the panel's four rows under five body lines");
    applied(controller, app::VisibleLines{1});
    expect(view_of(controller.frame()).rows.size() == 1, "a one line body still shows one row");
    const auto read = controller.explorer_view();
    expect(read.has_value() && read.value_or(app::ExplorerView{}).first_visible ==
                                   view_of(controller.frame()).first_visible,
           "the window reads the same panel view without a frame");
}

// 面に焦点がある間は IME を閉じたまま（NORMAL と同じ）、焦点が外れたらモードの規則に戻る
// （ADR 0061 の決定 1・E4 の追補）。窓の鍵の行き先は explorer_focused の安い読みで決める。
void verify_ime_and_focus()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    expect(!controller.explorer_focused() && controller.frame().ime == app::ImeStance::as_left,
           "a shown panel without the focus leaves the IME as the mode says");
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    expect(controller.explorer_focused() && controller.delivery().ime == app::ImeStance::closed,
           "the focused panel keeps the IME closed");
    static_cast<void>(key(editing, ExplorerKeyKind::esc));
    expect(!controller.explorer_focused() && controller.delivery().ime == app::ImeStance::as_left,
           "Esc gives the focus back and the IME follows the mode again");
    expect(app::ime_stance_of(core::EditMode::vim, core::VimMode::insert, std::nullopt, true) ==
                   app::ImeStance::closed &&
               app::ime_stance_of(core::EditMode::ordinary, core::VimMode::normal,
                                  app::CommandInput{core::CommandLine::empty()},
                                  true) == app::ImeStance::closed_once,
           "an open input line still decides the stance over the focused panel");
}

void verify_one_listing_at_a_time()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    static_cast<void>(key(editing, ExplorerKeyKind::enter));
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    static_cast<void>(key(editing, ExplorerKeyKind::enter));
    const auto git_ticket = editing.folders().requests().back().ticket;
    expect(editing.folders().entry_requests() == 2 &&
               editing.folders().requests().back().folder == path_of("C:\\proj\\.git"),
           "a second folder waits while the port lists the first");
    static_cast<void>(deliver(editing, {}, FolderProgress::complete));
    expect(editing.folders().entry_requests() == 3 &&
               editing.folders().requests().back().folder == path_of("C:\\proj\\src") &&
               editing.folders().requests().back().ticket == git_ticket + 1,
           "the waiting folder is listed when the first ends");
}

void verify_stale_and_root()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    static_cast<void>(key(editing, ExplorerKeyKind::l));
    const auto old = editing.folders().requests().back().ticket;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    const auto changed = run_ex(controller, "set root=C:\\other");
    expect(printed(changed) == "sel - first 0 focus 0" &&
               saved(editing).root == std::optional{path_of("C:\\other")} &&
               editing.folders().requests().back().folder == path_of("C:\\other"),
           "a new root rebuilds the tree, drops the selection and lists the new root at once");
    editing.folders().serve(
        app::FolderBatch{old, {}, FolderProgress::complete, {entry_of("C:\\proj\\src\\x", false)}});
    const auto stale = controller.apply_frame(app::WorkCompleted{});
    expect(printed(stale) == "sel - first 0 focus 0", "a batch of an older ticket is dropped");
    const auto fresh =
        deliver(editing, {entry_of("C:\\other\\b", false)}, FolderProgress::complete);
    expect(printed(fresh) == "b;sel - first 0 focus 0", "the new root's entries are placed");
}

void verify_unreadable()
{
    Editing editing(shown_at(project));
    applied(editing.controller(), app::VisibleLines{10});
    const auto failed = deliver(editing, {}, FolderProgress::failed);
    expect(message_of(failed) == "proj（読めません）" && printed(failed) == "sel - first 0 focus 0",
           "an unreadable folder keeps its rows and tells one line");
    static_cast<void>(key(editing, ExplorerKeyKind::j));
    expect(editing.folders().requests().size() == 1, "an unreadable folder is not listed again");
    Editing nested(shown_at(project));
    static_cast<void>(loaded_project(nested));
    static_cast<void>(key(nested, ExplorerKeyKind::j));
    static_cast<void>(key(nested, ExplorerKeyKind::j));
    static_cast<void>(key(nested, ExplorerKeyKind::l));
    const auto locked = deliver(nested, {}, FolderProgress::failed);
    expect(message_of(locked) == "src（読めません）" &&
               printed(locked) == ".git/+~;src/-;README.md;sel 1 first 0 focus 0",
           "an unreadable subfolder stays as an open folder row");
    expect(message_of(key(nested, ExplorerKeyKind::k)) == "-",
           "a panel key is a user operation and clears the notice");
}

void verify_truncated()
{
    Editing editing(shown_at(project));
    applied(editing.controller(), app::VisibleLines{10});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, FolderProgress::truncated};
    for (std::size_t index = 0; index <= core::ExplorerTree::entry_limit; ++index)
    {
        batch.entries.push_back(entry_of(std::format("C:\\proj\\f{:04}", index), false));
    }
    editing.folders().serve(std::move(batch));
    const auto frame = editing.controller().apply_frame(app::WorkCompleted{});
    // 面の行数は本文の 10 行 − ルート名の 1 行 = 9（E4 の追補）。
    expect(view_of(frame).total == core::ExplorerTree::entry_limit + 1 &&
               view_of(frame).rows.size() == 9 && view_of(frame).rows.front().name == "f0000",
           "the frame carries only the panel's rows of a large folder");
    const auto bottom = editing.controller().apply_frame(app::ScrollExplorer{5000});
    const auto rows = view_of(bottom).rows;
    expect(view_of(bottom).first_visible == core::ExplorerTree::entry_limit + 1 - 9 &&
               rows.size() == 9 && rows.back().more && rows.front().name == "f1992",
           "more entries than the limit end with the … row");
}

void verify_root_requests()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    const auto untitled = run_ex(controller, "set root=.");
    expect(message_of(untitled) == "E32: No file name" && editing.settings().writes() == 0,
           "set root=. without a file fails with one line");
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    applied(controller, app::OpenDocument{path_of("C:\\work\\a.txt")});
    const auto here = run_ex(controller, "set root=.");
    expect(message_of(here) == "root=C:\\work" &&
               saved(editing).root == std::optional{path_of("C:\\work")},
           "set root=. saves the folder of the active document");
    expect(editing.folders().requests().empty(), "a hidden panel lists nothing");
    const auto chosen = controller.apply_frame(app::ExplorerRootChosen{path_of("D:\\picked")});
    expect(message_of(chosen) == "root=D:\\picked" &&
               saved(editing).root == std::optional{path_of("D:\\picked")},
           "a chosen folder is saved as the root");
    const auto shown = run_ex(controller, "set explorer");
    expect(shown.explorer.has_value() && editing.folders().entry_requests() == 1 &&
               editing.folders().requests().back().folder == path_of("D:\\picked"),
           "set explorer shows the panel and lists the root");
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    const auto hidden = run_ex(controller, "set noexplorer");
    expect(printed(hidden) == "none" && saved(editing).explorer == false,
           "set noexplorer hides the panel");
    static_cast<void>(run_ex(controller, "set explorer"));
    expect(!view_of(controller.frame()).focused, "hiding the panel from Ex gave the focus back");
}

void verify_palette_shares_port()
{
    Editing editing(shown_at(project));
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::OpenDocument{path_of("C:\\work\\a.txt")});
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    const auto explorer_ticket = editing.folders().requests().back().ticket;
    const auto palette = controller.apply_frame(app::OpenCommandPalette{});
    const auto &requests = editing.folders().requests();
    expect(requests.size() == 2 && requests.back().listing == app::FolderListingKind::files &&
               requests.back().ticket == explorer_ticket + 1 && !view_of(palette).focused,
           "Ctrl+P takes the next ticket, lists files and takes the focus from the panel");
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    expect(requests.size() == 2, "the panel waits while the palette lists its folder");
    editing.folders().serve(app::FolderBatch{requests.back().ticket, {}, FolderProgress::complete});
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    expect(requests.size() == 3 && requests.back().folder == path_of(project) &&
               requests.back().listing == app::FolderListingKind::entries,
           "the cancelled root is listed again after the palette's listing ends");
    const auto frame = deliver(editing, {entry_of("C:\\proj\\a", false)}, FolderProgress::complete);
    expect(printed(frame).starts_with("a;"), "the panel loads after the palette");
}
} // namespace

void verify_explorer_contracts()
{
    verify_toggle_focus();
    verify_entries_round_trip();
    verify_keys();
    verify_enter_opens_file();
    verify_pointing();
    verify_pointing_across_arrival();
    verify_scroll();
    verify_panel_rows();
    verify_ime_and_focus();
    verify_one_listing_at_a_time();
    verify_stale_and_root();
    verify_unreadable();
    verify_truncated();
    verify_root_requests();
    verify_palette_shares_port();
}

void verify_explorer_scope()
{
    verify_explorer_contracts();
}
} // namespace nenenib::tests
