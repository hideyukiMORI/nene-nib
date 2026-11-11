// Issue #1397（ADR 0179・施主決定 D83）: プレビューのタブの契約
// `--transient-tab`。エクスプローラーの 面のクリック 1
// 回で開いたタブは印（題名が斜体）を持ち、次のクリックで同じ帯の位置に置き換わる。
// 置き換え・固定のきっかけ 5 つ・既に開いているファイル・保存済みでない印は置き換えない（守りの
// 二重化）・復元は印を覚えない・最近使った順・置き換えは履歴に書かない、を見る。port はどれも
// 替え玉で、時刻・スレッド・OS には触れない。
#include "Appearance.hpp"
#include "AutosaveId.hpp"
#include "CloseTab.hpp"
#include "Document.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "EditorState.hpp"
#include "EndSession.hpp"
#include "ExplorerKey.hpp"
#include "ExplorerKeyKind.hpp"
#include "ExplorerPointing.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderEntry.hpp"
#include "FolderProgress.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "PinTab.hpp"
#include "PointExplorer.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "SelectDocumentFormat.hpp"
#include "SelectEditMode.hpp"
#include "SessionEnd.hpp"
#include "SettleRecentTab.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::ExplorerPointing;
using app::PointExplorer;

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] SettingsReading shown_project()
{
    auto settings = core::default_editor_settings();
    settings.explorer = true;
    settings.root = path_of("C:\\proj");
    return settings;
}

// C:\proj に a.txt〜e.txt（面の行 0〜4）を置き、missing.txt（行 5）は読めない。
void load_project(Editing &editing)
{
    for (const char name : std::string_view{"abcde"})
    {
        const std::string file = std::string("C:\\proj\\") + name + ".txt";
        editing.files().hold_at(file, Bytes{std::string(1, name) + " body"});
    }
    applied(editing.controller(), app::VisibleLines{10});
    app::FolderBatch batch{
        editing.folders().requests().back().ticket, {}, app::FolderProgress::complete};
    for (const char name : std::string_view{"abcde"})
    {
        batch.entries.push_back(
            app::FolderEntry{path_of(std::string("C:\\proj\\") + name + ".txt"), false, false});
    }
    batch.entries.push_back(app::FolderEntry{path_of("C:\\proj\\missing.txt"), false, false});
    editing.folders().serve(std::move(batch));
    applied(editing.controller(), app::WorkCompleted{});
}

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

// 帯の題名を並べ、プレビューのタブに `~` を、アクティブに `*` を付ける（"a.txt~*|b.txt"）。
[[nodiscard]] std::string band(const app::EditorFrame &frame)
{
    std::string text;
    for (std::size_t index = 0; index < frame.tabs.size(); ++index)
    {
        text += index == 0 ? "" : "|";
        text += std::string(frame.tabs.at(index).title.text());
        text += frame.tabs.at(index).transient ? "~" : "";
        text += index == frame.active_tab ? "*" : "";
    }
    return text;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("-");
}

[[nodiscard]] std::size_t transient_count(const app::EditorFrame &frame)
{
    std::size_t count = 0;
    for (const auto &tab : frame.tabs)
    {
        count += tab.transient ? 1U : 0U;
    }
    return count;
}

// 置き換え（決定 3-2）。無題に開いて印を付け、次のクリックで同じ位置の中身が替わる。同じファイル
// は切り替えるだけ。置き換えは履歴を読まない・書かない（決定 5）。読めなければ元のまま（3-2）。
void verify_replace()
{
    Editing editing(shown_project());
    load_project(editing);
    const auto first = clicked(editing, 0);
    expect(band(first) == "a.txt~*" && first.lines.front().text == "a body",
           "a click opens the file in the blank untitled tab as the transient tab");
    const auto second = clicked(editing, 1);
    expect(band(second) == "b.txt~*" && second.lines.front().text == "b body",
           "the next click replaces the transient tab in place");
    const auto same = clicked(editing, 1);
    expect(band(same) == "b.txt~*" && editing.reads().requests().size() == 2,
           "a click on the file of the transient tab only switches to it and reads nothing");
    expect(editing.history().reads() == 0 && editing.history().writes() == 0,
           "replacing the transient tab never reads nor writes the history");
    const auto failed = clicked(editing, 5);
    expect(band(failed) == "b.txt~*" && message_of(failed) == "開けませんでした: missing.txt",
           "a click on a file that cannot be read leaves one line and the transient tab as it is");
    // ほかのタブを見ている間のクリックは、プレビューのタブの位置で置き換えて、そこへ切り替える
    // （決定 8）。ほかのタブは動かない。
    applied(editing.controller(), app::OpenDocument{path_of("C:\\proj\\e.txt")});
    expect(band(editing.controller().frame()) == "b.txt~|e.txt*",
           "Ctrl+O opens a pinned tab to the right");
    const auto elsewhere = clicked(editing, 2);
    expect(band(elsewhere) == "c.txt~*|e.txt" && editing.history().writes() == 0,
           "a click while another tab is active replaces the transient tab at its place");
    const auto open = clicked(editing, 4);
    expect(band(open) == "c.txt~|e.txt*",
           "a click on a file open in a pinned tab switches to it and keeps the mark");
    // 印のタブを × で閉じるのは普通の閉じる流れ（履歴に書く・決定 5）。
    applied(editing.controller(), app::CloseTab{0});
    expect(band(editing.controller().frame()) == "e.txt*" && editing.history().writes() == 1,
           "closing the transient tab is an ordinary close and records the history");
    const auto fresh = clicked(editing, 0);
    expect(band(fresh) == "e.txt|a.txt~*",
           "with no transient tab a click opens to the right of the active tab with the mark");
}

// 固定のきっかけ（決定 4）。未保存になった（本文・保存の形）・保存した・面のダブルクリックと
// Enter・題名のダブルクリック（PinTab）・明示の「開く」で同じファイル。
void verify_pins()
{
    Editing editing(shown_project());
    load_project(editing);
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 0));
    applied(controller, app::InsertText{"x"});
    expect(band(controller.frame()) == "● a.txt*", "typing pins the transient tab");
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    expect(band(controller.frame()) == "a.txt*",
           "undoing back to the saved point does not mark the tab again");
    expect(band(clicked(editing, 1)) == "a.txt|b.txt~*",
           "after pinning, the next click opens a new transient tab to the right");
    applied(controller, app::SelectDocumentFormat{core::TextEncoding::shift_jis});
    expect(transient_count(controller.frame()) == 0, "changing the encoding to save pins the tab");
    static_cast<void>(clicked(editing, 2));
    applied(controller, app::SaveDocument{path_of("C:\\proj\\c.txt"), core::TextEncoding::utf8});
    expect(band(controller.frame()) == "a.txt|● b.txt|c.txt*", "saving pins the transient tab");
    expect(band(double_clicked(editing, 3)) == "a.txt|● b.txt|c.txt|d.txt*",
           "a double click on a file in the panel opens it pinned");
    static_cast<void>(clicked(editing, 4));
    expect(band(controller.frame()) == "a.txt|● b.txt|c.txt|d.txt|e.txt~*", "a click marks e");
    applied(controller, app::ExplorerKey{app::ExplorerKeyKind::enter});
    expect(transient_count(controller.frame()) == 0,
           "Enter on the selected file of the transient tab pins it");
    applied(controller, app::CloseTab{4});
    static_cast<void>(clicked(editing, 4));
    applied(controller, app::PinTab{0});
    expect(transient_count(controller.frame()) == 1,
           "a double click on the title of another tab pins nothing");
    applied(controller, app::PinTab{4});
    expect(band(controller.frame()) == "a.txt|● b.txt|c.txt|d.txt|e.txt*",
           "a double click on the title of the transient tab pins it");
    applied(controller, app::CloseTab{4});
    static_cast<void>(clicked(editing, 4));
    const auto reads = editing.reads().requests().size();
    applied(controller, app::SwitchTab{0});
    applied(controller, app::OpenDocument{path_of("C:\\proj\\e.txt")});
    expect(band(controller.frame()) == "a.txt|● b.txt|c.txt|d.txt|e.txt*" &&
               editing.reads().requests().size() == reads,
           "opening the file of the transient tab explicitly switches to it and pins it");
}

// Vim の INSERT に入っただけでは固定しない（決定 11）。打てば固定する。
void verify_vim_insert()
{
    Editing editing(shown_project());
    load_project(editing);
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 0));
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::VimKeyPress{core::VimKey{core::VimCharacter{U'i'}}});
    applied(controller, app::VimKeyPress{core::VimKey{core::VimSpecialKey::escape}});
    expect(band(controller.frame()) == "a.txt~*",
           "entering and leaving INSERT without typing keeps the mark");
    applied(controller, app::VimKeyPress{core::VimKey{core::VimCharacter{U'x'}}});
    expect(transient_count(controller.frame()) == 0, "deleting a character in NORMAL pins the tab");
}

// 最近使った順（決定 6）。置き換えは外す文書を抜いて新しい文書を先頭に置く。
void verify_recency()
{
    Editing editing(shown_project());
    load_project(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of("C:\\proj\\d.txt")});
    applied(controller, app::OpenDocument{path_of("C:\\proj\\e.txt")});
    static_cast<void>(clicked(editing, 0));
    applied(controller, app::SwitchTab{0});
    expect(band(controller.frame()) == "d.txt*|e.txt|a.txt~", "a is marked right of e");
    static_cast<void>(clicked(editing, 1));
    expect(band(controller.frame()) == "d.txt|e.txt|b.txt~*", "b replaces a at its place");
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    const auto walked = controller.apply_frame(app::SettleRecentTab{});
    expect(band(walked) == "d.txt*|e.txt|b.txt~",
           "Ctrl+Tab from the replaced tab goes to the tab used just before it");
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    const auto twice = controller.apply_frame(app::SettleRecentTab{});
    expect(band(twice) == "d.txt|e.txt*|b.txt~",
           "the replaced document left the recency and e follows d");
}

// 復元（決定 7）。窓を閉じるとき印のタブは普通のタブとして覚え、次の起動で普通のタブで戻る。
void verify_restore()
{
    Editing editing(shown_project());
    load_project(editing);
    applied(editing.controller(), app::OpenDocument{path_of("C:\\proj\\d.txt")});
    static_cast<void>(clicked(editing, 0));
    applied(editing.controller(), app::EndSession{app::SessionEnd::window_closed});
    const auto &written = editing.session().written();
    expect(written.has_value() && written.value().tabs.size() == 2 &&
               written.value().tabs.at(1).path == path_of("C:\\proj\\a.txt"),
           "the transient tab is listed in the session like any other tab");
    Editing restored(SessionReading{written}, [](ScriptedFiles &files)
                     { files.hold_at("C:\\proj\\a.txt", Bytes{"a body"}); });
    const auto frame = restored.controller().frame();
    expect(frame.tabs.size() == 2 && transient_count(frame) == 0,
           "the restored tabs carry no mark");
}

// 守りの二重化（決定 2）。印の文書が保存済みでなければ置き換えず、右に新しいタブを足して印を移す。
// split（決定 9）は閉じると開くの合成で、もう 1 つの枠が印の文書を見ていれば新しい文書の先頭へ。
void verify_state_replacement()
{
    const auto opened = [](std::string_view path)
    { return app::Document{path_of(path), core::TextEncoding::utf8, std::size_t{0}}; };
    const auto start = app::EditorState::create(core::Appearance::dark, core::EditMode::ordinary)
                           .with_opened(core::TextBuffer::from_utf8(std::string_view{"a"}).value(),
                                        opened("C:\\proj\\a.txt"))
                           .with_active_transient();
    const auto replaced = start.with_transient_replaced(
        core::TextBuffer::from_utf8(std::string_view{"b"}).value(), opened("C:\\proj\\b.txt"));
    expect(replaced.tab_count() == 1 && replaced.document().path == path_of("C:\\proj\\b.txt") &&
               replaced.transient_tab() == std::size_t{0} &&
               replaced.transient() != start.transient(),
           "replacing keeps one tab and gives the new document a new number with the mark");
    const auto modified = start.with_document(
        app::Document{path_of("C:\\proj\\a.txt"), core::TextEncoding::utf8, std::nullopt});
    const auto kept = modified.with_transient_replaced(
        core::TextBuffer::from_utf8(std::string_view{"b"}).value(), opened("C:\\proj\\b.txt"));
    expect(kept.tab_count() == 2 && kept.active_tab() == 1 &&
               kept.transient_tab() == std::size_t{1} && kept.parked_bundle_at(0) != nullptr &&
               kept.parked_bundle_at(0)->document.path == path_of("C:\\proj\\a.txt"),
           "a mark on a document that is not saved is not replaced; a new tab takes the mark");
    const auto split = start.with_split(core::SplitLayout::vertical);
    const auto shown = split.with_transient_replaced(
        core::TextBuffer::from_utf8(std::string_view{"b"}).value(), opened("C:\\proj\\b.txt"));
    expect(shown.tab_count() == 1 && shown.other_pane() != nullptr &&
               shown.other_pane()->tab == shown.autosave_ids().at(0),
           "the other pane that showed the transient document shows the new document");
    expect(start.with_closed(0).transient() == start.transient() &&
               start.with_new_tab().with_closed(0).transient() == std::nullopt,
           "closing the transient tab drops the mark; the last tab is not closed");
}

void verify_split_click()
{
    Editing editing(shown_project());
    load_project(editing);
    auto &controller = editing.controller();
    static_cast<void>(clicked(editing, 0));
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    const auto replaced = clicked(editing, 1);
    expect(band(replaced) == "b.txt~*" && replaced.lines.front().text == "b body",
           "a click with both panes on the transient document replaces it");
}
} // namespace

void verify_transient_tab_contracts()
{
    verify_replace();
    verify_pins();
    verify_vim_insert();
    verify_recency();
    verify_restore();
    verify_state_replacement();
    verify_split_click();
}

void verify_transient_tab_scope()
{
    verify_transient_tab_contracts();
}
} // namespace nenenib::tests
