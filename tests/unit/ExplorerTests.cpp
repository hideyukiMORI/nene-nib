// Issue #997（ADR 0162 の決定 3〜6・9）: エクスプローラーの application の契約 `--explorer`。
// 意図 4 つと焦点の 3 状態、FolderPort の entries の往復と古い券、読めないフォルダの 1 行、
// ファイルを開く open_listed の道、設定の保存、Ctrl+P の面と port を分け合う順番を見る。
// 面の幅のドラッグ（ResizeExplorer・追補 D78）は動かす間は書かず、離したときと戻したときだけ書く。
// port はどれも替え玉で、時刻・スレッド・OS には触れない。
#include "CaretMotion.hpp"
#include "CommandInput.hpp"
#include "CommandLine.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExplorerKey.hpp"
#include "ExplorerKeyKind.hpp"
#include "ExplorerPointing.hpp"
#include "ExplorerResizing.hpp"
#include "ExplorerRootChosen.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerTree.hpp"
#include "ExplorerView.hpp"
#include "ExplorerWidth.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderListingKind.hpp"
#include "FolderProgress.hpp"
#include "GiantLine.hpp"
#include "HoverExplorer.hpp"
#include "ImeStance.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "PointExplorer.hpp"
#include "PointTitleBar.hpp"
#include "ResizeExplorer.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedFolders.hpp"
#include "ScriptedSettings.hpp"
#include "ScrollExplorer.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SettingsFailure.hpp"
#include "SettleRecentTab.hpp"
#include "SyntaxRole.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarTarget.hpp"
#include "TitleBarWidth.hpp"
#include "ToggleExplorer.hpp"
#include "VimMode.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
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
    static_cast<void>(key(editing, ExplorerKeyKind::down));
    static_cast<void>(key(editing, ExplorerKeyKind::down));
    const auto walked = key(editing, ExplorerKeyKind::down);
    const auto reads = editing.reads().requests().size();
    expect(printed(walked) == ".git/+~;src/+;README.md;sel 2 first 0 focus 1" && reads == 0 &&
               walked.document.title.text() != "README.md",
           "walking onto a file with the arrow keys does not open it (#1388)");
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
    expect(printed(twice) == printed(again) && printed(after) == printed(twice),
           "the second press of a double click after a click on the folder does not toggle it "
           "again and its release does nothing (#1388)");
    const auto outside = controller.apply_frame(PointExplorer{9, ExplorerPointing::double_click});
    expect(printed(outside) == printed(twice), "a row outside the rows does nothing");
}

// 1 回のクリック（ADR 0162 の決定 4 の改め・#1388）。窓は面の上の速い 2 回の押しを
// 「押す → 離す → DBLCLK → 離す」と送る（nib_window_tests --explorer の verify_click_messages）。
app::EditorFrame clicked(Editing &editing, std::size_t row)
{
    static_cast<void>(
        editing.controller().apply_frame(PointExplorer{row, ExplorerPointing::pressed}));
    return editing.controller().apply_frame(PointExplorer{row, ExplorerPointing::released});
}

app::EditorFrame double_clicked(Editing &editing, std::size_t row)
{
    static_cast<void>(clicked(editing, row));
    static_cast<void>(
        editing.controller().apply_frame(PointExplorer{row, ExplorerPointing::double_click}));
    return editing.controller().apply_frame(PointExplorer{row, ExplorerPointing::released});
}

void verify_click_opens_file()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    editing.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    static_cast<void>(editing.controller().apply_frame(app::ToggleExplorer{}));
    const auto opened = clicked(editing, 2);
    expect(opened.document.title.text() == "README.md" && !opened.lines.empty() &&
               opened.lines.front().text == "read me" && editing.reads().requests().size() == 1,
           "a click on a file opens it through the read port");
    expect(printed(opened) == ".git/+~;src/+;README.md;sel 2 first 0 focus 1",
           "a click on a file selects it and keeps the focus in the panel");
    expect(editing.history().reads() == 0 && editing.history().writes() == 0,
           "a click on a file never touches the history");
    const auto walked = key(editing, ExplorerKeyKind::k);
    expect(printed(walked) == ".git/+~;src/+;README.md;sel 1 first 0 focus 1",
           "after a click the arrow keys still walk the panel");
    static_cast<void>(editing.controller().apply_frame(app::NewTab{}));
    const auto switched = clicked(editing, 2);
    expect(switched.document.title.text() == "README.md" && switched.tabs.size() == 2 &&
               editing.reads().requests().size() == 1 && message_of(switched) == "-",
           "a click on a file already open switches to its tab without reading it again");
    expect(clicked(editing, 9).document.title.text() == "README.md",
           "a click outside the rows does nothing");

    Editing missing(shown_at(project));
    static_cast<void>(loaded_project(missing));
    static_cast<void>(missing.controller().apply_frame(app::ToggleExplorer{}));
    const auto notice = clicked(missing, 2);
    expect(message_of(notice) == "開けませんでした: README.md" &&
               printed(notice) == ".git/+~;src/+;README.md;sel 2 first 0 focus 1" &&
               missing.history().reads() == 0 && missing.history().writes() == 0,
           "a click on a file that cannot be opened leaves one notice line and the focus");
}

// ダブルクリック（#1388）。1 打目のクリックで開いているので、2 打目は焦点を本文へ移すだけ。
// 2 回は読まない。
void verify_double_click_reads_once()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    editing.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    static_cast<void>(editing.controller().apply_frame(app::ToggleExplorer{}));
    const auto opened = double_clicked(editing, 2);
    expect(opened.document.title.text() == "README.md" && editing.reads().requests().size() == 1,
           "a double click on a file reads it once");
    expect(printed(opened) == ".git/+~;src/+;README.md;sel 2 first 0 focus 0",
           "a double click on a file gives the focus to the text and keeps the panel");
    Editing missing(shown_at(project));
    static_cast<void>(loaded_project(missing));
    static_cast<void>(missing.controller().apply_frame(app::ToggleExplorer{}));
    const auto first = clicked(missing, 2);
    const auto tried = missing.reads().requests().size();
    static_cast<void>(
        missing.controller().apply_frame(PointExplorer{2, ExplorerPointing::double_click}));
    const auto notice =
        missing.controller().apply_frame(PointExplorer{2, ExplorerPointing::released});
    expect(message_of(first) == "開けませんでした: README.md" &&
               missing.reads().requests().size() == tried && missing.history().writes() == 0 &&
               printed(notice) == ".git/+~;src/+;README.md;sel 2 first 0 focus 0",
           "a double click on a file that cannot be opened tries once and leaves one line");
    // 1 打目が効かなかった（押した行と離した行が違った）2 打目は、1 回のクリックと Enter と同じ。
    Editing late(shown_at(project));
    static_cast<void>(loaded_project(late));
    late.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    static_cast<void>(late.controller().apply_frame(PointExplorer{1, ExplorerPointing::pressed}));
    static_cast<void>(late.controller().apply_frame(PointExplorer{2, ExplorerPointing::released}));
    const auto second =
        late.controller().apply_frame(PointExplorer{2, ExplorerPointing::double_click});
    expect(second.document.title.text() == "README.md" && late.reads().requests().size() == 1 &&
               printed(second) == ".git/+~;src/+;README.md;sel 2 first 0 focus 0",
           "a second press whose first click did not count opens the file once like Enter");
}

// フォルダはクリック 1 回で開閉し、ダブルクリックでも 1 回だけ（開いて閉じる、にしない・#1388）。
void verify_folder_double_click_toggles_once()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    static_cast<void>(editing.controller().apply_frame(app::ToggleExplorer{}));
    const auto opened = double_clicked(editing, 1);
    expect(printed(opened) == ".git/+~;src/-;README.md;sel 1 first 0 focus 1" &&
               editing.folders().entry_requests() == 2,
           "a double click on a closed folder opens it once and keeps the focus in the panel");
    static_cast<void>(
        deliver(editing, {entry_of("C:\\proj\\src\\main.cpp", false)}, FolderProgress::complete));
    const auto closed = double_clicked(editing, 1);
    expect(printed(closed) == ".git/+~;src/+;README.md;sel 1 first 0 focus 1" &&
               editing.folders().entry_requests() == 2,
           "a double click on an open folder closes it once");
    const auto reopened = clicked(editing, 1);
    expect(printed(reopened) == ".git/+~;src/-;.main.cpp;README.md;sel 1 first 0 focus 1",
           "a single click opens the folder again");
    expect(editing.reads().requests().empty(), "clicking folders reads no file");
}

// ファイル path を読んだ回数（読みの port の要求の数）。
[[nodiscard]] std::size_t reads_of(Editing &editing, std::string_view path)
{
    std::size_t count = 0;
    for (const auto &request : editing.reads().requests())
    {
        count += request.path == path_of(path) ? 1U : 0U;
    }
    return count;
}

// 2 打目の 1 通（DBLCLK と離し）。
app::EditorFrame second_press(Editing &editing, std::size_t row)
{
    static_cast<void>(
        editing.controller().apply_frame(PointExplorer{row, ExplorerPointing::double_click}));
    return editing.controller().apply_frame(PointExplorer{row, ExplorerPointing::released});
}

// 1 打目と 2 打目の間に裏の列挙で上に行が足され、2 打目の画面の行が別の節を指しても、2 打目は
// 1 打目の節として扱う（レビュー V-1391 の M-1 の (b)）。
void verify_double_click_across_arrival()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    editing.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    editing.files().hold_at("C:\\proj\\src\\main.cpp", Bytes{"int main"});
    static_cast<void>(clicked(editing, 1));
    static_cast<void>(clicked(editing, 2));
    static_cast<void>(
        deliver(editing, {entry_of("C:\\proj\\src\\main.cpp", false)}, FolderProgress::complete));
    const auto second = second_press(editing, 2);
    expect(printed(second) == ".git/+~;src/-;.main.cpp;README.md;sel 3 first 0 focus 0",
           "a second press after children arrived above stays on the clicked file");
    expect(second.document.title.text() == "README.md" &&
               reads_of(editing, "C:\\proj\\README.md") == 1 &&
               reads_of(editing, "C:\\proj\\src\\main.cpp") == 0,
           "the clicked file is read once and the row moved under the pointer is not read");
}

// 末尾までスクロールした面で開いたフォルダをダブルクリックすると、1 打目の畳みで先頭が切り詰め
// られて行が画面上でずれる。2 打目は 1 打目の節として扱い、ほかの行を開閉しない（M-1 の (a)）。
void verify_double_click_collapse_at_bottom()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 1));
    static_cast<void>(
        deliver(editing, {entry_of("C:\\proj\\src\\main.cpp", false)}, FolderProgress::complete));
    // 面は窓の 4 行 − ルート名の 1 行 = 3 行。見えている行は 4 行なので、末尾の先頭は 1。
    applied(controller, app::VisibleLines{10, {}, 0, 0, 4});
    const auto bottom = controller.apply_frame(app::ScrollExplorer{100});
    expect(printed(bottom) == "src/-;.main.cpp;README.md;sel 1 first 1 focus 1",
           "the panel is scrolled to its last rows with the open folder on the first line");
    const auto requests = editing.folders().entry_requests();
    static_cast<void>(clicked(editing, 1));
    const auto collapsed = controller.frame();
    expect(printed(collapsed) == ".git/+~;src/+;README.md;sel 1 first 0 focus 1",
           "the first press closes the folder and the first row is pulled up");
    // 2 打目は同じ画面の点。先頭が 0 になったので、画面の行は .git を指す。
    const auto second = second_press(editing, 0);
    expect(printed(second) == printed(collapsed) &&
               editing.folders().entry_requests() == requests && editing.reads().requests().empty(),
           "the second press closes nothing else, opens nothing and reads no file");
}

// 1 打目と 2 打目の間に使う人の操作（タブの切り替え）が入ったら、2 打目は画面の行を引いて 1 回の
// クリックとして効かせ、そのファイルをアクティブにする。読みは増えない（レビュー V-1391 の R-1）。
void verify_double_click_after_tab_switch()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    editing.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    editing.files().hold_at("C:\\work\\b.txt", Bytes{"bee"});
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of("C:\\work\\b.txt")});
    const auto opened = clicked(editing, 2);
    expect(opened.document.title.text() == "README.md" && opened.tabs.size() == 2,
           "a click opens the file in a second tab");
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    const auto walked = controller.apply_frame(app::SettleRecentTab{});
    expect(walked.document.title.text() == "b.txt", "Ctrl+Tab goes back to the other tab");
    const auto second = second_press(editing, 2);
    expect(second.document.title.text() == "README.md" && second.tabs.size() == 2 &&
               reads_of(editing, "C:\\proj\\README.md") == 1 &&
               printed(second) == ".git/+~;src/+;README.md;sel 2 first 0 focus 0",
           "a second press after a tab switch makes the clicked file active without reading it");
}

// 1 打目で開いたファイルが巨大な行の問いを出したとき（ADR 0140 の決定 2）。窓は同じ配達の中で
// モーダルに聞いて答えを意図で返すので、2 打目は答えの後に来る。2 打目は読み直さず、もう一度
// 聞かない（レビュー V-1391 の F-1）。
void verify_double_click_after_question()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    const std::string giant = "head\n" + std::string(core::giant_line_threshold + 1, 'x') + "\n";
    editing.files().hold_at("C:\\proj\\README.md", Bytes{giant});
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(PointExplorer{2, ExplorerPointing::pressed}));
    const auto asked = controller.apply(PointExplorer{2, ExplorerPointing::released});
    expect(asked.display_question, "a click on a giant line file raises the question");
    static_cast<void>(controller.apply(app::SelectDisplay{core::DisplayForm::text}));
    const auto pressed = controller.apply(PointExplorer{2, ExplorerPointing::double_click});
    const auto second = controller.apply_frame(PointExplorer{2, ExplorerPointing::released});
    expect(!pressed.display_question && second.document.title.text() == "README.md" &&
               reads_of(editing, "C:\\proj\\README.md") == 1 && second.tabs.size() == 1 &&
               second.display == core::DisplayForm::text &&
               printed(second) == ".git/+~;src/+;README.md;sel 2 first 0 focus 0",
           "a second press after the answer reads nothing, asks nothing and gives the focus");
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

// 窓が面の行数を送る前（explorer_lines が 0）は、面の行数は本文の見えている行数 − 1（ルート名の
// 行・最小 1）を application の 1 か所で導き、選んだ行は面の最後の行より下へ出ない（E4 の追補・
// ADR 0162 の決定 2）。
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

// 面の行数は窓が client の高さから数えた面の行数 − 1（ルート名の行）で、本文の見えている行数
// （上下の分割では半分の高さ・ADR 0148）に依らない（#1125・ADR 0162 の追補）。
void verify_panel_rows_from_window()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    // 上下の分割の操作中の枠は 4 行、面は窓の帯の全高で 9 行（ルート名を含む）。
    applied(controller, app::VisibleLines{4, {}, 0, 0, 9});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, FolderProgress::complete};
    for (std::size_t index = 0; index < 20; ++index)
    {
        batch.entries.push_back(entry_of(std::format("C:\\proj\\h{:02}.txt", index), false));
    }
    editing.folders().serve(std::move(batch));
    const auto listed = controller.apply_frame(app::WorkCompleted{});
    expect(view_of(listed).rows.size() == 8 && view_of(listed).total == 20,
           "the panel shows the window's lines less the root line, not the split pane's");
    bool inside = true;
    for (std::size_t step = 0; step < 12; ++step)
    {
        const auto view = view_of(key(editing, ExplorerKeyKind::j));
        inside = inside && view.rows.size() == 8 && view.selected.has_value() &&
                 view.selected.value() < view.first_visible + 8;
    }
    expect(inside, "the selection follows inside the panel's eight rows");
    expect(view_of(controller.frame()).first_visible == 4,
           "the first row follows the selection by the panel's rows");
    applied(controller, app::VisibleLines{30, {}, 0, 0, 9});
    expect(view_of(controller.frame()).rows.size() == 8,
           "more body lines do not change the panel's rows");
    applied(controller, app::VisibleLines{4, {}, 0, 0, 1});
    expect(view_of(controller.frame()).rows.size() == 1, "a one line panel still shows one row");
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

// 面に焦点がある間の IME の変換と確定は本文へ入れない（composition_ignored・E4 の追補）。焦点を
// 本文へ戻せば今までどおり入る。
void verify_composition_while_focused()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    const auto before = controller.text().size_bytes();
    static_cast<void>(
        controller.apply_frame(app::ComposeText{core::Composition{"あ", {}, core::Offset{0}}}));
    expect(!controller.is_composing(), "the focused panel shows no composition in the body");
    static_cast<void>(controller.apply_frame(app::CommitText{"あ"}));
    expect(controller.text().size_bytes() == before,
           "a commit while the panel has the focus is dropped");
    static_cast<void>(key(editing, ExplorerKeyKind::esc));
    static_cast<void>(controller.apply_frame(app::CommitText{"あ"}));
    expect(controller.text().size_bytes() == before + 3, "after Esc the commit reaches the body");
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
[[nodiscard]] app::ResizeExplorer resized(app::ExplorerResizing resizing, std::int32_t dips)
{
    return app::ResizeExplorer{resizing, dips};
}

// 窓の読み（explorer_width）と frame の面の幅（ExplorerView.width）の DIP。面が消えていれば -1、
// 既定の式なら 0。
[[nodiscard]] std::int32_t width_of(const app::EditorController &controller)
{
    const auto width = controller.explorer_width();
    return width.has_value() ? width.value().dips.value_or(0) : -1;
}

[[nodiscard]] std::int32_t width_of(const app::EditorFrame &frame)
{
    return frame.explorer.has_value() ? view_of(frame).width.dips.value_or(0) : -1;
}

// 面の幅のドラッグ（追補 D78）。動かしている間は状態だけ、離したら 1 回書く、ダブルクリックは
// 既定の式へ戻して書く。動かさずに離したら書かない。
void verify_resize_writes_on_release()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    expect(width_of(controller) == 0 && width_of(controller.frame()) == 0,
           "a settings file without explorer_width uses the default formula");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, 300)));
    expect(editing.settings().writes() == 0 && width_of(controller) == 0,
           "releasing the divider without dragging writes nothing");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, 300)));
    const auto dragging = controller.apply_frame(resized(app::ExplorerResizing::dragged, 320));
    expect(editing.settings().writes() == 0 && width_of(controller) == 320 &&
               width_of(dragging) == 320 && !dragging.settings.explorer_width.has_value(),
           "dragging shows the width without writing the settings");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, 330)));
    expect(editing.settings().writes() == 1 && saved(editing).explorer_width == 330 &&
               width_of(controller) == 330 && width_of(controller.frame()) == 330,
           "releasing writes the released width once");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, 330)));
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, 330)));
    expect(editing.settings().writes() == 1, "the same width is not written again");
    const auto reset = controller.apply_frame(resized(app::ExplorerResizing::reset, 0));
    expect(editing.settings().writes() == 2 && !saved(editing).explorer_width.has_value() &&
               width_of(reset) == 0,
           "a double click goes back to the default formula and writes once");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::reset, 0)));
    expect(editing.settings().writes() == 2, "resetting the default again writes nothing");
}

// 覚えた幅で起動する・面が消えていれば何もしない・書けなければ覚えた幅へ戻して知らせる（D78）。
void verify_resize_remembered_and_hidden()
{
    auto remembered = core::default_editor_settings();
    remembered.explorer = true;
    remembered.explorer_width = 400;
    Editing editing(SettingsReading{remembered});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    expect(width_of(controller) == 400 && width_of(controller.frame()) == 400,
           "the remembered width is used at start");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, 250)));
    editing.settings().fail(app::SettingsFailure::unwritable);
    const auto failed = controller.apply_frame(resized(app::ExplorerResizing::released, 250));
    expect(message_of(failed) == "Settings could not be saved" && width_of(controller) == 400,
           "a width that cannot be written is told and the remembered width stays");
    editing.settings().fail(std::nullopt);
    const auto cleared = controller.apply_frame(resized(app::ExplorerResizing::dragged, 260));
    expect(message_of(cleared) == "-",
           "dragging is a user operation and clears the one-intent message");
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    const auto writes = editing.settings().writes();
    expect(width_of(controller) == -1 && width_of(controller.frame()) == -1,
           "a hidden panel has no width");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, 300)));
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, 300)));
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::reset, 0)));
    expect(editing.settings().writes() == writes && width_of(controller) == -1,
           "a hidden panel ignores the divider");
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    expect(width_of(controller) == 400, "a panel shown again has the remembered width");
}

// controller の入口で幅を [120, 9999] に収める（#1380 の R-1）。窓を経ない送り手でも、書いた値を
// 自分の読みで既定に戻さず、INT32_MAX や負の値で溢れない。
void verify_resize_clamped()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, 50)));
    expect(width_of(controller) == 120 && width_of(controller.frame()) == 120,
           "a width below the minimum drags as 120");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, 50)));
    expect(editing.settings().writes() == 1 && saved(editing).explorer_width == 120,
           "a width below the minimum is written as 120");
    constexpr auto largest = std::numeric_limits<std::int32_t>::max();
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, largest)));
    const auto widest = controller.frame();
    expect(width_of(controller) == 9999 && width_of(widest) == 9999,
           "INT32_MAX drags as 9999 and the frame is built");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, largest)));
    expect(editing.settings().writes() == 2 && saved(editing).explorer_width == 9999,
           "INT32_MAX is written as 9999");
    constexpr auto smallest = std::numeric_limits<std::int32_t>::min();
    for (const std::int32_t negative : {-5, smallest})
    {
        static_cast<void>(
            controller.apply_frame(resized(app::ExplorerResizing::dragged, negative)));
        expect(width_of(controller) == 120, "a negative width drags as 120");
    }
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, smallest)));
    expect(editing.settings().writes() == 3 && saved(editing).explorer_width == 120,
           "a negative width is written as 120");
}

// Ctrl+Tab の歩きの途中のドラッグは使う人の操作で、歩きを確定する（#1380 の T-2・ADR 0058）。
void verify_resize_settles_tab_walk()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    for (const std::string_view body : {"b", "c", "d"})
    {
        applied(controller, app::NewTab{});
        applied(controller, app::InsertText{std::string(body)});
    }
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    expect(controller.tab_walking(), "the walk is open before the drag");
    const auto dragged = controller.apply_frame(resized(app::ExplorerResizing::dragged, 300));
    expect(!controller.tab_walking() && width_of(dragged) == 300 && !dragged.lines.empty() &&
               dragged.lines.front().text == "c",
           "dragging the divider settles the walk on the walked tab");
    const auto walked = controller.apply_frame(app::WalkRecentTab{core::TabStep::next});
    expect(!walked.lines.empty() && walked.lines.front().text == "d",
           "the settled walk put the walked tab first in the order");
}

// ドラッグの途中で面が閉じたら、ドラッグは終わりで幅は書かない（Issue #1380 の設計席の決め）。
// 次に開いたときは覚えた幅で、開き直した後に離しても書かない。
void verify_resize_hidden_while_dragging()
{
    auto remembered = core::default_editor_settings();
    remembered.explorer = true;
    remembered.explorer_width = 400;
    Editing editing(SettingsReading{remembered});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::dragged, 250)));
    expect(width_of(controller) == 250, "the drag shows its width");
    for (std::size_t toggles = 0; toggles < 2 && width_of(controller) != -1; ++toggles)
    {
        static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    }
    expect(width_of(controller) == -1 && saved(editing).explorer_width == 400,
           "closing the panel during a drag keeps the remembered width in the settings");
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    const auto writes = editing.settings().writes();
    expect(width_of(controller) == 400 && width_of(controller.frame()) == 400,
           "the panel shown again has the remembered width, not the dropped drag");
    static_cast<void>(controller.apply_frame(resized(app::ExplorerResizing::released, 250)));
    expect(editing.settings().writes() == writes && width_of(controller) == 400 &&
               saved(editing).explorer_width == 400,
           "a release after the panel came back writes nothing");
}

// マウスを載せた行（#1390・ADR 0056 の決定 3 の帯と同じ形）。画面の行で覚えるので、ホイールで行が
// 送られると載った木の行も変わる。見えている行の外・面の外（値なし）・面を消したときは消える。
// 使う人の操作ではないので、知らせとダブルクリックの 2 打目と組む印を消さない。
void verify_hover()
{
    Editing editing(shown_at(project));
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, {}, 0, 0, 4});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, FolderProgress::complete};
    for (std::size_t index = 0; index < 6; ++index)
    {
        batch.entries.push_back(entry_of(std::format("C:\\proj\\f{}.txt", index), false));
    }
    editing.folders().serve(std::move(batch));
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    const auto over = controller.apply_frame(app::HoverExplorer{1});
    expect(view_of(over).hovered == std::optional<std::size_t>{1} &&
               controller.explorer_hovered() == std::optional<std::size_t>{1},
           "the hovered screen line is the tree row under it");
    const auto scrolled = controller.apply_frame(app::ScrollExplorer{2});
    expect(view_of(scrolled).first_visible == 2 &&
               view_of(scrolled).hovered == std::optional<std::size_t>{3},
           "after the wheel the row under the pointer is the hovered one");
    const auto past = controller.apply_frame(app::HoverExplorer{5});
    expect(!view_of(past).hovered.has_value(), "a line below the rows hovers nothing");
    const auto below = controller.apply(app::HoverExplorer{6});
    expect(!below.repaint && controller.explorer_hovered() == std::optional<std::size_t>{6},
           "moving between lines below the rows keeps the line but does not repaint");
    expect(controller.apply(app::HoverExplorer{0}).repaint,
           "moving from below the rows onto a row repaints");
    const auto left = controller.apply_frame(app::HoverExplorer{std::nullopt});
    expect(!view_of(left).hovered.has_value() && !controller.explorer_hovered().has_value(),
           "leaving the panel clears the hovered row");
    static_cast<void>(controller.apply_frame(app::HoverExplorer{0}));
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    static_cast<void>(controller.apply_frame(app::ToggleExplorer{}));
    expect(!controller.explorer_hovered().has_value(), "hiding the panel clears the hovered row");
    const auto hidden = controller.apply_frame(app::HoverExplorer{1});
    expect(!hidden.explorer.has_value() && !controller.explorer_hovered().has_value(),
           "a hidden panel ignores the pointer");

    Editing missing(shown_at(project));
    static_cast<void>(loaded_project(missing));
    static_cast<void>(missing.controller().apply_frame(app::ToggleExplorer{}));
    static_cast<void>(clicked(missing, 2));
    const auto kept = missing.controller().apply_frame(app::HoverExplorer{0});
    expect(message_of(kept) == "開けませんでした: README.md",
           "hovering a row keeps the notice of the last click");
    Editing folder(shown_at(project));
    static_cast<void>(loaded_project(folder));
    static_cast<void>(clicked(folder, 1));
    static_cast<void>(folder.controller().apply_frame(app::HoverExplorer{2}));
    const auto second = second_press(folder, 1);
    expect(printed(second) == ".git/+~;src/-;README.md;sel 1 first 0 focus 1",
           "a hover between the clicks of a double click does not toggle the folder back");
}

// マウスを動かすだけの意図（面の載せた行・帯の載せた要素）と帯の幅の知らせは、通常モードの ↑ ↓ が
// 狙う横位置を捨てない（レビュー V-1428 の M-1・F-3・D82: メモ帳はマウスを動かしても狙いの桁を
// 変えない）。2 行目の短い行を通っても 3 行目は 1 行目と同じ桁に着く。
void verify_pointer_keeps_wanted_column()
{
    const auto down =
        app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse};
    const auto moved_after = [&](const app::EditorIntent &between)
    {
        Editing editing(shown_at(project));
        auto &controller = editing.controller();
        applied(controller, app::InsertText{"abcdef\nx\nabcdef"});
        applied(controller, app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}, 0, 0, 4});
        applied(controller,
                app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{5}},
                                core::SelectionAnchoring::collapse});
        applied(controller, down);
        applied(controller, between);
        return controller.apply_frame(down).caret.position ==
               core::TextPosition{core::LineNumber{3}, core::Column{5}};
    };
    expect(moved_after(app::HoverExplorer{1}),
           "hovering a panel row between two downward moves keeps the wanted x");
    expect(moved_after(app::PointTitleBar{core::TitleBarTarget{core::TitleBarHit::tab, 0}}),
           "hovering the tab band between two downward moves keeps the wanted x");
    expect(moved_after(app::TitleBarWidth{800}),
           "the band width notice between two downward moves keeps the wanted x");
}

// アクティブな文書の行と未保存の印の出どころ（#1390）。アクティブは今の文書と同じファイル、未保存は
// 開いているタブのどれか（アクティブか脇の束）が未保存のファイル。印の色の役割は名前の言語。
void verify_row_marks()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    editing.files().hold_at("C:\\proj\\README.md", Bytes{"read me"});
    auto &controller = editing.controller();
    const auto before = view_of(controller.frame());
    expect(before.rows.size() == 3 && !before.rows.at(2).active && !before.rows.at(2).unsaved &&
               before.rows.at(2).mark == std::optional{core::SyntaxRole::string} &&
               !before.rows.at(1).mark.has_value() && !before.rows.at(0).mark.has_value(),
           "a Markdown file is the string mark and folders carry none");
    static_cast<void>(key(editing, ExplorerKeyKind::down));
    static_cast<void>(key(editing, ExplorerKeyKind::down));
    static_cast<void>(key(editing, ExplorerKeyKind::down));
    const auto opened = view_of(key(editing, ExplorerKeyKind::enter));
    expect(opened.rows.at(2).active && !opened.rows.at(2).unsaved && !opened.rows.at(1).active,
           "the active document's row is active and saved");
    applied(controller, app::InsertText{"a"});
    const auto typed = view_of(controller.frame());
    expect(typed.rows.at(2).active && typed.rows.at(2).unsaved,
           "typing into the active document marks its row unsaved");
    applied(controller, app::NewTab{});
    const auto parked = view_of(controller.frame());
    expect(!parked.rows.at(2).active && parked.rows.at(2).unsaved,
           "a parked unsaved tab keeps the unsaved mark and is no longer active");
}

// 1 打目と 2 打目の間の面のホイールは、2 打目と組む印を消さない（レビュー V-1391b の R-a）。
// フォルダは 1 回開くだけで、開いて閉じる、にしない。
void verify_double_click_across_wheel()
{
    Editing editing(shown_at(project));
    static_cast<void>(loaded_project(editing));
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 1));
    static_cast<void>(controller.apply_frame(app::ScrollExplorer{1}));
    const auto second = second_press(editing, 1);
    expect(printed(second) == ".git/+~;src/-;README.md;sel 1 first 0 focus 1" &&
               editing.folders().entry_requests() == 2,
           "a wheel notch between the clicks keeps the folder open after the double click");
}

// 1 打目で開けなかったファイルのダブルクリックは、2 打目（DBLCLK と続く離し）の後も「開けません
// でした」の 1 行を残す（レビュー V-1391b の R-b）。押しは新しいクリックなので消す。
void verify_double_click_keeps_notice()
{
    Editing missing(shown_at(project));
    static_cast<void>(loaded_project(missing));
    static_cast<void>(missing.controller().apply_frame(app::ToggleExplorer{}));
    const auto first = clicked(missing, 2);
    const auto tried = missing.reads().requests().size();
    const auto second = second_press(missing, 2);
    expect(message_of(first) == "開けませんでした: README.md" &&
               message_of(second) == "開けませんでした: README.md" &&
               missing.reads().requests().size() == tried,
           "the notice of a file that cannot be opened survives the second press");
    const auto pressed =
        missing.controller().apply_frame(PointExplorer{1, ExplorerPointing::pressed});
    expect(message_of(pressed) == "-", "a new press clears the notice");
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
    verify_click_opens_file();
    verify_double_click_reads_once();
    verify_folder_double_click_toggles_once();
    verify_double_click_across_arrival();
    verify_double_click_collapse_at_bottom();
    verify_double_click_after_tab_switch();
    verify_double_click_after_question();
    verify_scroll();
    verify_panel_rows();
    verify_panel_rows_from_window();
    verify_ime_and_focus();
    verify_composition_while_focused();
    verify_one_listing_at_a_time();
    verify_stale_and_root();
    verify_unreadable();
    verify_truncated();
    verify_root_requests();
    verify_palette_shares_port();
    verify_resize_writes_on_release();
    verify_resize_remembered_and_hidden();
    verify_resize_clamped();
    verify_resize_settles_tab_walk();
    verify_resize_hidden_while_dragging();
    verify_hover();
    verify_pointer_keeps_wanted_column();
    verify_row_marks();
    verify_double_click_across_wheel();
    verify_double_click_keeps_notice();
}

void verify_explorer_scope()
{
    verify_explorer_contracts();
}
} // namespace nenenib::tests
