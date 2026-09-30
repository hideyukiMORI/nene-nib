// scope `--tabs` の単体テスト（ADR 0042 決定 2・ADR 0056）。状態・切り替え・閉じる（決定 1〜4・
// 6・7）と、開く（決定 5）と起動（決定 13）。
#include "ActivateCommandChoice.hpp"
#include "Appearance.hpp"
#include "CancelCommand.hpp"
#include "CloseTab.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "ComposeText.hpp"
#include "DevicePixels.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenTabList.hpp"
#include "PaletteOrigin.hpp"
#include "PointTitleBar.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "ScrollLines.hpp"
#include "ScrollTabs.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionPresence.hpp"
#include "SettleRecentTab.hpp"
#include "StoreVimRegister.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TabCommand.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "TabKey.hpp"
#include "TabKeyTable.hpp"
#include "TabRecency.hpp"
#include "TabStep.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "TitleBarWidth.hpp"
#include "UnsavedTab.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimMode.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRegisterText.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::CloseTab;
using nenenib::application::ComposeText;
using nenenib::application::EditorFrame;
using nenenib::application::EditorPorts;
using nenenib::application::EditorState;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::NewTab;
using nenenib::application::next_unsaved_tab;
using nenenib::application::OpenDocument;
using nenenib::application::PointTitleBar;
using nenenib::application::ScrollLines;
using nenenib::application::ScrollTabs;
using nenenib::application::SelectEditMode;
using nenenib::application::SettleRecentTab;
using nenenib::application::SwitchTab;
using nenenib::application::tab_unsaved;
using nenenib::application::title_bar_input;
using nenenib::application::TitleBarWidth;
using nenenib::application::VisibleLines;
using nenenib::application::WalkRecentTab;
using nenenib::core::Appearance;
using nenenib::core::EditMode;
using nenenib::core::FilePath;
using nenenib::core::HistoryDirection;
using nenenib::core::LayoutRect;
using nenenib::core::SaveState;
using nenenib::core::TabCommand;
using nenenib::core::TabKey;
using nenenib::core::TabStep;
using nenenib::core::TitleBarHit;
using nenenib::core::TitleBarInput;
using nenenib::core::TitleBarTarget;
using nenenib::core::VimMode;

[[nodiscard]] bool titled(const EditorFrame &frame, std::size_t tab, std::string_view title)
{
    return frame.tabs.size() > tab && frame.tabs.at(tab).title.text() == title;
}

// 帯の並びを本文の 1 行目で読む（どのタブがどこにあるか）。
[[nodiscard]] std::string first_line(const EditorFrame &frame)
{
    return frame.lines.empty() ? std::string{} : frame.lines.front().text;
}

// 本文が a b c d の 4 本のタブ。アクティブは右端の d。
void open_four_tabs(EditorController &controller)
{
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    for (const std::string_view body : {"b", "c", "d"})
    {
        applied(controller, NewTab{});
        applied(controller, InsertText{std::string(body)});
    }
}

// NewTab は空の無題をアクティブの右に足し、そこへ切り替える（決定 3）。
void verify_new_tab()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto single = controller.frame();
    expect(single.tabs.size() == 1 && single.active_tab == 0 && titled(single, 0, "無題") &&
               !single.closing,
           "the window starts with one untitled tab");
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"alpha"});
    const auto added = controller.apply(NewTab{});
    expect(added.tabs.size() == 2 && added.active_tab == 1,
           "NewTab adds a tab to the right and activates it");
    expect(vim_body(added).empty() && added.document.title.text() == "無題" &&
               added.document.save_state == SaveState::saved,
           "the new tab is an empty, saved, untitled document");
    expect(titled(added, 1, "無題") && titled(added, 0, "● 無題") &&
               added.tabs.at(0).save_state == SaveState::modified,
           "the band lists both tabs and the parked one keeps its unsaved mark");
    applied(controller, SwitchTab{0});
    const auto middle = controller.apply(NewTab{});
    expect(middle.tabs.size() == 3 && middle.active_tab == 1 && titled(middle, 2, "無題") &&
               titled(middle, 0, "● 無題"),
           "NewTab inserts right of the active tab, not at the end");
}

// 本文・選択・スクロール・保存の状態は文書ごと（決定 1・2）。
void verify_switch_keeps_each_document()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("a1\na2\na3\na4\na5\na6")});
    applied(controller, VisibleLines{3});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, InsertText{"x"});
    applied(controller, ScrollLines{2});
    applied(controller, NewTab{});
    applied(controller, InsertText{"b"});
    const auto back = controller.apply(SwitchTab{0});
    expect(back.active_tab == 0 && back.first_visible.value == 3 && back.total_lines == 6 &&
               caret_at(back, 1, 2),
           "switching back restores the body, the caret and the scroll of that document");
    expect(back.document.title.text() == "● note.txt" && titled(back, 1, "● 無題") &&
               back.document.path.has_value(),
           "each tab keeps its own path and unsaved mark");
    const auto other = controller.apply(SwitchTab{1});
    expect(vim_body(other) == "b" && caret_at(other, 1, 2) && other.first_visible.value == 1,
           "the other document keeps its own body, caret and scroll");
}

// undo は文書ごとに独立し、切り替えの前後で単位が閉じる（決定 4）。
void verify_undo_per_document()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    applied(controller, NewTab{});
    applied(controller, InsertText{"b"});
    applied(controller, SwitchTab{0});
    applied(controller, InsertText{"c"});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "a",
           "undo after switching back takes away only what was typed after the switch");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "and the next undo reaches this document's own first edit");
    expect(controller.frame().document.save_state == SaveState::saved,
           "undoing to the saved position clears this document's mark");
    applied(controller, SwitchTab{1});
    expect(vim_body(controller.frame()) == "b" && titled(controller.frame(), 0, "無題"),
           "the other tab's body is untouched and the parked view shows the saved state");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "the other tab undoes its own edit");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "and has nothing more to undo");
}

// 閉じた後のアクティブ（決定 6）。範囲の外は何もしない。
void verify_close_moves_active()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, SwitchTab{1});
    const auto right = controller.apply(CloseTab{1});
    expect(right.tabs.size() == 3 && right.active_tab == 1 && first_line(right) == "c",
           "closing the active tab activates its right neighbour");
    const auto left_closed = controller.apply(CloseTab{0});
    expect(left_closed.tabs.size() == 2 && left_closed.active_tab == 0 &&
               first_line(left_closed) == "c",
           "closing a tab left of the active one keeps the active document");
    applied(controller, SwitchTab{1});
    const auto last = controller.apply(CloseTab{1});
    expect(last.tabs.size() == 1 && last.active_tab == 0 && first_line(last) == "c",
           "closing the rightmost active tab activates its left neighbour");
    const auto outside = controller.apply(CloseTab{5});
    expect(outside.tabs.size() == 1 && !outside.closing && first_line(outside) == "c",
           "closing a position outside the band does nothing");
    expect(controller.apply(SwitchTab{3}).active_tab == 0,
           "switching to a position outside the band does nothing");
}

void verify_close_right_of_active()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, SwitchTab{0});
    const auto closed = controller.apply(CloseTab{2});
    expect(closed.tabs.size() == 3 && closed.active_tab == 0 && first_line(closed) == "a" &&
               titled(closed, 2, "● 無題"),
           "closing a tab right of the active one keeps the active position");
}

// 最後の 1 つは状態を変えず closing を立てる。次の意図で下りる（D22）。
void verify_last_tab_closes_window()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"keep"});
    const auto closing = controller.apply(CloseTab{0});
    expect(closing.closing && closing.tabs.size() == 1 && vim_body(closing) == "keep" &&
               closing.document.save_state == SaveState::modified,
           "closing the last tab asks the window to close and changes nothing else");
    expect(!controller.apply(ScrollLines{0}).closing, "the request lasts for that intent only");
}

// 切り替えは Vim の保留・回数・VISUAL・INSERT を捨て、窓全体の値を残す（決定 4）。
void verify_vim_switch_discards_pending()
{
    Editing editing;
    open_vim_document(editing, "one two\nthree");
    EditorController &controller = editing.controller();
    vim_replay(controller, "/two<CR>0yiwqavl");
    const auto added = controller.apply(NewTab{});
    const auto &vim = controller.vim_state();
    expect(vim.mode == VimMode::normal && vim.macro_recording.has_value() &&
               added.recording == std::optional<char>{'a'},
           "VISUAL ends but the macro keeps recording across the switch");
    expect(vim.last_search.has_value() && vim.unnamed_register.text == "one",
           "the last search and the registers belong to the window");
    vim_replay(controller, "p");
    expect(vim_body(controller.frame()) == "one", "a yank in one tab puts in another");
    vim_replay(controller, "2d");
    const auto back = controller.apply(SwitchTab{0});
    expect(!controller.vim_state().count.has_value() && !controller.vim_state().pending.has_value(),
           "the count and the pending operator are dropped");
    expect(vim_body(back) == "one two\nthree" && caret_at(back, 1, 2) &&
               back.lines.at(0).selection.presence == nenenib::core::SelectionPresence::absent,
           "the VISUAL selection comes back collapsed to its caret");
    vim_replay(controller, "q");
    expect(!controller.vim_state().macro_recording.has_value(), "the recording stops with q");
}

void verify_vim_switch_leaves_insert()
{
    Editing editing;
    open_vim_document(editing, "one two\nthree");
    EditorController &controller = editing.controller();
    applied(controller, NewTab{});
    vim_replay(controller, "ione<Esc>");
    applied(controller, SwitchTab{0});
    vim_replay(controller, "A!");
    const auto away = controller.apply(SwitchTab{1});
    expect(controller.vim_state().mode == VimMode::normal &&
               !controller.vim_state().insert_repeat.has_value() && vim_body(away) == "one",
           "switching out of INSERT lands in NORMAL");
    const auto back = controller.apply(SwitchTab{0});
    expect(vim_body(back) == "one two!\nthree" && caret_at(back, 1, 8),
           "the NORMAL caret rests on the last character of the line");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "one two\nthree",
           "the INSERT typed before the switch is one undo unit of its own document");
    applied(controller, SwitchTab{1});
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()).empty(), "the other tab undoes its own insert");
}

// 欲しい列と 'scroll' は文書ごと。何を捨てるかは engine の純関数が決める（決定 4）。
void verify_vim_switched_document()
{
    auto state = empty_vim_state();
    state.mode = VimMode::visual;
    state.count = nenenib::core::VimCount{3};
    state.wanted_column = nenenib::core::VimWantedColumn{nenenib::core::VimColumnWish::at_line_end,
                                                         nenenib::core::VirtualColumn{0}};
    state.scroll_lines = nenenib::core::VimCount{4};
    state.last_macro = 'q';
    const auto switched = nenenib::core::vim_switched_document(
        state, std::nullopt, std::optional{nenenib::core::VimCount{7}});
    expect(switched.mode == VimMode::normal && !switched.count.has_value(),
           "the mode rests and the count is dropped");
    expect(!switched.wanted_column.has_value() &&
               switched.scroll_lines == std::optional{nenenib::core::VimCount{7}},
           "the wanted column and 'scroll' come from the other document");
    expect(switched.last_macro == std::optional<char>{'q'}, "the window-wide values stay");
}

// 入力行と IME の変換中の文字列は切り替えで閉じる（決定 4）。
void verify_switch_closes_input()
{
    Editing editing;
    open_vim_document(editing, "one two\nthree");
    EditorController &controller = editing.controller();
    applied(controller, NewTab{});
    vim_replay(controller, "/on");
    expect(controller.command_line_active(), "the search line is open before the switch");
    const auto searched = controller.apply(SwitchTab{0});
    expect(!searched.command_line.has_value() && !controller.command_line_active() &&
               caret_at(searched, 1, 1),
           "switching cancels the search line and its preview");
    vim_replay(controller, ":");
    expect(!controller.apply(WalkRecentTab{TabStep::next}).command_line.has_value(),
           "switching cancels the Ex line");
    applied(controller, SelectEditMode{EditMode::ordinary});
    applied(controller, ComposeText{composed_of("あ", {}, 0)});
    const auto composed = controller.apply(SwitchTab{0});
    expect(!composed.composition.has_value() && vim_body(composed) == "one two\nthree",
           "switching drops the composition without typing it");
}

// 状態の写しはほかのタブの束の参照だけを写す（決定 2）。
void verify_parked_references()
{
    auto state = EditorState::create(Appearance::dark, EditMode::ordinary).with_new_tab();
    state = state.with_new_tab();
    const auto first = state.parked().at(0);
    const auto second = state.parked().at(1);
    const auto edited = state.with_edit(
        buffer_of("x"), nenenib::core::collapsed_at(nenenib::core::Offset{1}), state.history());
    expect(edited.parked().at(0) == first && edited.parked().at(1) == second,
           "editing the active document shares the parked bundles");
    const auto switched = state.with_switched(1);
    expect(switched.tab_count() == 3 && switched.active_tab() == 1 &&
               switched.parked().at(0) == first && switched.parked().at(1) != second,
           "switching keeps the untouched tab's bundle and parks the old active one");
    const auto closed = switched.with_closed(0);
    expect(closed.parked().size() == 1 && closed.active_tab() == 0 &&
               closed.parked().at(0) == switched.parked().at(1),
           "closing a parked tab keeps the other bundles as they are");
}
// 契約の経路。どれも絶対パスの形（起動引数とダイアログが作る値と同じ）。
[[nodiscard]] FilePath path_of(std::string_view text)
{
    auto parsed = FilePath::parse(text);
    expect(parsed.has_value(), "the contract path parses");
    return std::move(parsed).value();
}

[[nodiscard]] OpenDocument open_at(std::string_view text)
{
    return OpenDocument{path_of(text)};
}

// a.txt b.txt c.txt の中身をそれぞれ a b c にする。
void hold_three(ScriptedFiles &files)
{
    files.hold_at("C:\\work\\a.txt", Bytes{std::string("a")});
    files.hold_at("C:\\work\\b.txt", Bytes{std::string("b")});
    files.hold_at("C:\\work\\c.txt", Bytes{std::string("c")});
}

// (a) 同じファイルはそのタブへ切り替え、読み直さない（決定 5）。
void verify_open_same_file()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one")});
    applied(controller, VisibleLines{10});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, InsertText{"x"});
    const auto again = controller.apply(OpenDocument{sample_path()});
    expect(again.tabs.size() == 1 && again.active_tab == 0 && vim_body(again) == "xone" &&
               again.document.title.text() == "● note.txt" && editing.files().reads() == 1,
           "opening the active file again keeps its unsaved body and mark without rereading");
    applied(controller, NewTab{});
    const auto back = controller.apply(OpenDocument{sample_path()});
    expect(back.tabs.size() == 2 && back.active_tab == 0 && vim_body(back) == "xone" &&
               titled(back, 0, "● note.txt") && titled(back, 1, "無題") &&
               editing.files().reads() == 1,
           "opening a file open in another tab switches to that tab");
}

// (a) の判定は FilePort が決める。替え玉が同じと答えた組は同じファイル。
void verify_open_same_file_by_port()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one")});
    editing.files().treat_as_same("C:\\WORK\\NOTE.TXT", "C:\\work\\note.txt");
    applied(controller, open_at("C:\\WORK\\NOTE.TXT"));
    applied(controller, NewTab{});
    const auto frame = controller.apply(OpenDocument{sample_path()});
    expect(frame.tabs.size() == 2 && frame.active_tab == 0 && titled(frame, 0, "NOTE.TXT") &&
               editing.files().reads() == 1,
           "a path the port calls the same file switches instead of opening a second tab");
}

// (b) 何も書いていない無題に開く。1 文字でも編集があれば（取り消しても）新しいタブ。
void verify_open_into_blank_untitled()
{
    Editing blank;
    blank.files().hold(Bytes{std::string("one")});
    const auto into = blank.controller().apply(OpenDocument{sample_path()});
    expect(into.tabs.size() == 1 && into.document.title.text() == "note.txt",
           "a blank untitled tab takes the opened file");
    Editing typed;
    typed.files().hold(Bytes{std::string("one")});
    applied(typed.controller(), InsertText{"a"});
    const auto beside = typed.controller().apply(OpenDocument{sample_path()});
    expect(beside.tabs.size() == 2 && beside.active_tab == 1 && titled(beside, 0, "● 無題") &&
               vim_body(beside) == "one",
           "an untitled tab with an edit keeps its text and the file opens in a new tab");
    Editing undone;
    undone.files().hold(Bytes{std::string("one")});
    applied(undone.controller(), InsertText{"a"});
    applied(undone.controller(), HistoryAction{HistoryDirection::undo});
    const auto kept = undone.controller().apply(OpenDocument{sample_path()});
    expect(kept.tabs.size() == 2 && kept.active_tab == 1,
           "an untitled tab whose edit was undone still has history and is not reused");
}

// (c) ほかのファイルはアクティブの右に入る。
void verify_open_right_of_active()
{
    Editing editing;
    EditorController &controller = editing.controller();
    hold_three(editing.files());
    applied(controller, VisibleLines{10});
    applied(controller, open_at("C:\\work\\a.txt"));
    const auto second = controller.apply(open_at("C:\\work\\b.txt"));
    expect(second.tabs.size() == 2 && second.active_tab == 1 && vim_body(second) == "b" &&
               titled(second, 0, "a.txt"),
           "a file opened from a file tab gets a new tab to the right");
    applied(controller, SwitchTab{0});
    const auto middle = controller.apply(open_at("C:\\work\\c.txt"));
    expect(middle.tabs.size() == 3 && middle.active_tab == 1 && titled(middle, 0, "a.txt") &&
               titled(middle, 1, "c.txt") && titled(middle, 2, "b.txt") &&
               middle.document.save_state == SaveState::saved,
           "the new tab goes right of the active tab, not at the end");
}

// 開けなかったらタブを足さず、失敗は今までどおり 1 意図だけ載る。
void verify_open_failure_adds_no_tab()
{
    Editing editing;
    EditorController &controller = editing.controller();
    hold_three(editing.files());
    applied(controller, open_at("C:\\work\\a.txt"));
    applied(controller, NewTab{});
    applied(controller, InsertText{"b"});
    const auto failed = controller.apply(open_at("C:\\work\\missing.txt"));
    expect(failed.tabs.size() == 2 && failed.active_tab == 1 && vim_body(failed) == "b" &&
               failed.document.last_failure == FileFailure::not_found,
           "a file that cannot be read adds no tab and reports the failure");
    expect(!controller.apply(VisibleLines{10}).document.last_failure.has_value(),
           "the failure stays for one intent");
    Editing blank;
    const auto untitled = blank.controller().apply(open_at("C:\\work\\missing.txt"));
    expect(untitled.tabs.size() == 1 && untitled.document.title.text() == "無題" &&
               untitled.document.last_failure == FileFailure::not_found,
           "a failed open leaves the blank untitled tab as it was");
}

// 起動の controller を組んで最初の表示値を返す（決定 13）。ポートは controller より先に宣言する。
[[nodiscard]] EditorFrame started(ScriptedFiles &files, const std::vector<OpenDocument> &initial)
{
    ScriptedAppearance appearance{Reading{Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    ScriptedSession session;
    const EditorController controller(
        EditorPorts{appearance, clipboard, files, code_pages, settings, themes, session}, initial);
    return controller.frame();
}

// 起動引数のファイルは全部を順にタブで開き、最後に開けたものがアクティブ（決定 13）。
void verify_startup_opens_every_file()
{
    ScriptedFiles files;
    hold_three(files);
    const auto three = started(files, {open_at("C:\\work\\a.txt"), open_at("C:\\work\\b.txt"),
                                       open_at("C:\\work\\c.txt")});
    expect(three.tabs.size() == 3 && three.active_tab == 2 && titled(three, 0, "a.txt") &&
               titled(three, 1, "b.txt") && titled(three, 2, "c.txt") && vim_body(three) == "c",
           "three arguments open three tabs and the last one is active");
    const auto skipped =
        started(files, {open_at("C:\\work\\a.txt"), open_at("C:\\work\\missing.txt"),
                        open_at("C:\\work\\c.txt")});
    expect(skipped.tabs.size() == 2 && skipped.active_tab == 1 && titled(skipped, 0, "a.txt") &&
               titled(skipped, 1, "c.txt") &&
               skipped.document.last_failure == FileFailure::not_found,
           "an argument that cannot be opened makes no tab and its failure reaches the window");
    const auto none = started(files, {});
    expect(none.tabs.size() == 1 && none.document.title.text() == "無題" &&
               !none.document.last_failure.has_value(),
           "no argument starts with one untitled tab");
    const auto twice = started(files, {open_at("C:\\work\\a.txt"), open_at("C:\\work\\a.txt")});
    expect(twice.tabs.size() == 1 && twice.document.title.text() == "a.txt",
           "the same file given twice opens one tab");
}

// 窓を閉じるときに確かめる未保存のタブは帯の左から順（#237）。保存済みのタブは飛ばし、
// 切り替えても帯の並びと印は変わらないので、確かめた次の位置から探し直せば 1 本ずつ 1 度だけ映る。
void verify_unsaved_tabs_in_band_order()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto single = controller.frame();
    expect(!next_unsaved_tab(single.tabs, 0).has_value(),
           "one saved tab asks nothing when the window closes");
    open_four_tabs(controller);
    applied(controller, SwitchTab{1});
    const auto band = controller.apply(NewTab{});
    expect(band.tabs.size() == 5 &&
               next_unsaved_tab(band.tabs, 0) == std::optional<std::size_t>{0} &&
               next_unsaved_tab(band.tabs, 2) == std::optional<std::size_t>{3} &&
               !next_unsaved_tab(band.tabs, 5).has_value() &&
               !next_unsaved_tab(band.tabs, 9).has_value(),
           "the next unsaved tab is searched rightward and skips the saved one");
    std::vector<std::size_t> asked;
    std::size_t from = 0;
    for (auto next = next_unsaved_tab(controller.frame().tabs, from); next.has_value();
         next = next_unsaved_tab(controller.frame().tabs, from))
    {
        const auto shown = controller.apply(SwitchTab{next.value()});
        expect(shown.active_tab == next.value() && shown.document.save_state == SaveState::modified,
               "the tab asked about is the one the window shows");
        asked.push_back(next.value());
        from = next.value() + 1;
    }
    expect(asked == std::vector<std::size_t>{0, 1, 3, 4},
           "every unsaved tab is asked once from the left of the band");
}
// ---------------------------------------------------------------- 帯（ADR 0056 の決定 2・3・7・8）

// 9 本のタブ。帯の幅 1200 DIP では 9 本目があふれ、送り量の上限は 150 DIP。
void open_nine_tabs(EditorController &controller)
{
    applied(controller, VisibleLines{10});
    for (std::size_t added = 0; added < 8; ++added)
    {
        applied(controller, NewTab{});
    }
}

// 帯の幅と送り量は状態に入り表示値に出る。切り替え・新しいタブ・閉じる・帯の幅のたびに
// アクティブなタブが見える所へ直り、ホイールは端で止まる。
void verify_band_scroll()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto initial = controller.frame();
    expect(initial.tab_scroll == 0 && !initial.hovered.has_value(),
           "the band starts unscrolled with nothing hovered");
    open_nine_tabs(controller);
    expect(controller.frame().tab_scroll == 0, "an unknown band width keeps the scroll at 0");
    expect(controller.apply(ScrollTabs{-1}).tab_scroll == 0,
           "the wheel does nothing before the band width is known");
    expect(controller.apply(TitleBarWidth{1200}).tab_scroll == 150,
           "the band width scrolls the active last tab into view");
    expect(controller.apply(SwitchTab{0}).tab_scroll == 0,
           "switching to the first tab scrolls it into view");
    expect(controller.apply(ScrollTabs{-1}).tab_scroll == 122, "one notch scrolls one tab");
    expect(controller.apply(ScrollTabs{-3}).tab_scroll == 150, "the wheel stops at the end");
    expect(controller.apply(WalkRecentTab{TabStep::next}).tab_scroll == 150,
           "a visible active tab keeps the scroll");
    expect(controller.apply(NewTab{}).tab_scroll == 272, "a new tab at the end is scrolled in");
    expect(controller.apply(CloseTab{9}).tab_scroll == 150,
           "closing clamps the scroll to the shorter strip");
    expect(controller.apply(TitleBarWidth{2000}).tab_scroll == 0,
           "a band wide enough for every tab does not scroll");
}

// マウスを載せた要素は状態に入り表示値に出る。閉じたタブの hover は消える（決定 2）。
void verify_band_hover()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    const TitleBarTarget third{TitleBarHit::tab, 2};
    expect(controller.apply(PointTitleBar{third}).hovered == std::optional{third},
           "the pointed element is in the frame");
    const auto closed_other = controller.apply(CloseTab{1});
    expect(closed_other.hovered == std::optional{third},
           "closing another tab keeps the hover where the pointer is");
    static_cast<void>(controller.apply(PointTitleBar{TitleBarTarget{TitleBarHit::tab_close, 1}}));
    const auto closed_hovered = controller.apply(CloseTab{1});
    expect(!closed_hovered.hovered.has_value(), "closing the hovered tab forgets the hover");
    static_cast<void>(controller.apply(PointTitleBar{TitleBarTarget{TitleBarHit::add_tab, 0}}));
    expect(!controller.apply(PointTitleBar{std::nullopt}).hovered.has_value(),
           "leaving the band clears the hover");
}

// ui が帯を描き・当て・閉じるときに読む純関数（ADR 0056 の決定 6・8・9）。配置の入力は表示値から
// 1 本で作り、hover に写すのは見た目の変わる要素だけ、窓の最小の大きさでタブ 1 本はあふれない。
void verify_band_pointer()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, NewTab{});
    applied(controller, TitleBarWidth{1200});
    const TitleBarTarget closer{TitleBarHit::tab_close, 1};
    const auto frame = controller.apply(PointTitleBar{closer});
    const TitleBarInput input = title_bar_input(frame, 1800, 144);
    expect(input.width == 1800 && input.dpi == 144 && input.tab_count == 5 && input.active == 4 &&
               input.scroll_dips == frame.tab_scroll && input.hovered == std::optional{closer},
           "the band input is the frame's tabs, active tab, scroll and hover");
    expect(tab_unsaved(frame.tabs, 0) && tab_unsaved(frame.tabs, 3) &&
               !tab_unsaved(frame.tabs, 4) && !tab_unsaved(frame.tabs, 9),
           "only a modified tab in range asks before it closes");
    const auto layout = nenenib::core::title_bar_layout(input);
    expect(nenenib::core::tab_hovered(layout, 1) && !nenenib::core::tab_hovered(layout, 0) &&
               !nenenib::core::tab_hovered(layout, 4) && !nenenib::core::tab_hovered(layout, 7),
           "the hovered tab is the one under the pointer, its close button included");
    expect(layout.tab_padding == 21 && layout.button_radius == 6,
           "the title padding and the button corner scale with the DPI");
    const auto hover = [](TitleBarHit hit, std::size_t tab)
    { return nenenib::core::title_bar_hover(TitleBarTarget{hit, tab}); };
    expect(hover(TitleBarHit::tab, 2) == std::optional{TitleBarTarget{TitleBarHit::tab, 2}} &&
               hover(TitleBarHit::tab_close, 1) == std::optional{closer} &&
               hover(TitleBarHit::add_tab, 0) ==
                   std::optional{TitleBarTarget{TitleBarHit::add_tab, 0}} &&
               hover(TitleBarHit::tab_list, 0) ==
                   std::optional{TitleBarTarget{TitleBarHit::tab_list, 0}},
           "tabs, close buttons and the band buttons are hovered");
    expect(!hover(TitleBarHit::caption, 0).has_value() &&
               !hover(TitleBarHit::minimize, 0).has_value() &&
               !hover(TitleBarHit::maximize, 0).has_value() &&
               !hover(TitleBarHit::close, 0).has_value() &&
               !hover(TitleBarHit::none, 0).has_value(),
           "the caption, the window buttons and outside the band clear the hover");
    expect(nenenib::core::minimum_window(96) == LayoutRect{0, 0, 360, 200} &&
               nenenib::core::minimum_window(144) == LayoutRect{0, 0, 540, 300},
           "the smallest window is 360 by 200 DIP");
    expect(
        !nenenib::core::title_bar_layout(TitleBarInput{360, 96, 1, 0, 0, std::nullopt}).overflowing,
        "one tab does not overflow in the smallest window");
    expect(nenenib::core::to_dips(540, 144) == 360 && nenenib::core::to_dips(1200, 96) == 1200 &&
               nenenib::core::to_dips(181, 120) == 144,
           "physical pixels become DIP for the band width");
}

// 押した要素と離した要素が同じ（種類も帯の位置も）ときだけ離した要素が返る（ADR 0056 の決定 9）。
void verify_band_release()
{
    using nenenib::core::title_bar_released;
    const TitleBarTarget close_two{TitleBarHit::tab_close, 2};
    expect(title_bar_released(close_two, close_two) == std::optional{close_two} &&
               title_bar_released(TitleBarTarget{TitleBarHit::add_tab, 0},
                                  TitleBarTarget{TitleBarHit::add_tab, 0}) ==
                   std::optional{TitleBarTarget{TitleBarHit::add_tab, 0}},
           "releasing on the pressed element returns it");
    expect(!title_bar_released(TitleBarTarget{TitleBarHit::tab, 2}, close_two).has_value() &&
               !title_bar_released(TitleBarTarget{TitleBarHit::caption, 0},
                                   TitleBarTarget{TitleBarHit::add_tab, 0})
                    .has_value(),
           "releasing on another kind of element does nothing");
    expect(!title_bar_released(TitleBarTarget{TitleBarHit::tab_close, 1}, close_two).has_value(),
           "releasing on the same kind of element of another tab does nothing");
    expect(!title_bar_released(std::nullopt, close_two).has_value(),
           "releasing without a press on the band does nothing");
}

// 右が viewport で欠けた最初のタブの帯の位置。無ければ本数。
[[nodiscard]] std::size_t right_clipped_tab(const nenenib::core::TitleBarLayout &layout)
{
    for (std::size_t tab = 0; tab < layout.tab_count; ++tab)
    {
        if (nenenib::core::tab_visible(layout, tab) &&
            nenenib::core::tab_rect(layout, tab).right > layout.viewport.right)
        {
            return tab;
        }
    }
    return layout.tab_count;
}

// 送る前にタブ tab の本体・送った後にその × に当たる点の x。無ければ値なし。
[[nodiscard]] std::optional<std::int32_t>
point_moved_onto_close(const nenenib::core::TitleBarLayout &before,
                       const nenenib::core::TitleBarLayout &after, std::size_t tab, std::int32_t y)
{
    const TitleBarTarget body{TitleBarHit::tab, tab};
    const TitleBarTarget closer{TitleBarHit::tab_close, tab};
    for (std::int32_t x = before.viewport.right - 1; x >= before.viewport.left; --x)
    {
        if (nenenib::core::title_bar_target(before, x, y) == body &&
            nenenib::core::title_bar_target(after, x, y) == closer)
        {
            return x;
        }
    }
    return std::nullopt;
}

// 右が欠けたタブの見えている右寄りを押すと切り替えで帯が左へ送られ、同じ点がそのタブの × に
// 来る。押した要素（タブ）と離した要素（×）は違うので閉じない（ADR 0056 の決定 9）。
void verify_band_release_after_scroll()
{
    const TitleBarInput unscrolled{1200, 96, 9, 0, 0, std::nullopt};
    const auto before = nenenib::core::title_bar_layout(unscrolled);
    const std::size_t clipped = right_clipped_tab(before);
    expect(before.overflowing && clipped < before.tab_count,
           "nine tabs in 1200 DIP leave a tab clipped on the right");
    TitleBarInput switched = unscrolled;
    switched.active = clipped;
    switched.scroll_dips = nenenib::core::tabs_scrolled_into_view(switched);
    const auto after = nenenib::core::title_bar_layout(switched);
    expect(after.scroll > 0, "switching to the clipped tab scrolls the band left");
    const auto rect = nenenib::core::tab_rect(before, clipped);
    const std::int32_t y = rect.top + ((rect.bottom - rect.top) / 2);
    const auto x = point_moved_onto_close(before, after, clipped, y);
    expect(x.has_value(), "a point on the clipped tab lands on its close button after the scroll");
    const auto pressed = nenenib::core::title_bar_target(before, x.value_or(0), y);
    const auto released = nenenib::core::title_bar_target(after, x.value_or(0), y);
    expect(!nenenib::core::title_bar_released(pressed, released).has_value(),
           "pressing the tab and releasing on its scrolled close button does not close it");
}

// 状態から読む配置の入力は、表示値から作る入力と 6 つの欄で一致する（ADR 0056 の決定 9）。
[[nodiscard]] bool same_band_input(const EditorController &controller)
{
    const TitleBarInput read = controller.title_bar_input(1800, 144);
    const TitleBarInput built = title_bar_input(controller.frame(), 1800, 144);
    return read.width == built.width && read.dpi == built.dpi &&
           read.tab_count == built.tab_count && read.active == built.active &&
           read.scroll_dips == built.scroll_dips && read.hovered == built.hovered;
}

void verify_band_input_from_state()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(same_band_input(controller), "one tab reads the same band input as the frame");
    open_nine_tabs(controller);
    expect(same_band_input(controller), "several tabs read the same band input as the frame");
    applied(controller, TitleBarWidth{1200});
    expect(controller.frame().tab_scroll > 0 && same_band_input(controller),
           "a scrolled band reads the same band input as the frame");
    applied(controller, SwitchTab{2});
    expect(same_band_input(controller), "a switched tab reads the same band input as the frame");
    applied(controller, PointTitleBar{TitleBarTarget{TitleBarHit::tab_close, 4}});
    expect(controller.frame().hovered.has_value() && same_band_input(controller),
           "a hovered band reads the same band input as the frame");
    applied(controller, CloseTab{8});
    expect(same_band_input(controller), "a closed tab reads the same band input as the frame");
}

// 5 つの鍵 × 2 つのモードの 10 通り（ADR 0056 の決定 10）。Vim モードの Ctrl+W だけが値なし。
void verify_tab_keys()
{
    using nenenib::core::tab_command_for;
    using Row = std::pair<TabKey, std::optional<TabCommand>>;
    const std::array<Row, 5> ordinary{{
        {TabKey::control_t, TabCommand::open},
        {TabKey::control_tab, TabCommand::next},
        {TabKey::control_shift_tab, TabCommand::previous},
        {TabKey::control_f4, TabCommand::close},
        {TabKey::control_w, TabCommand::close},
    }};
    const std::array<Row, 5> vim{{
        {TabKey::control_t, TabCommand::open},
        {TabKey::control_tab, TabCommand::next},
        {TabKey::control_shift_tab, TabCommand::previous},
        {TabKey::control_f4, TabCommand::close},
        {TabKey::control_w, std::nullopt},
    }};
    for (const auto &[key, command] : ordinary)
    {
        expect(tab_command_for(key, EditMode::ordinary) == command,
               "a tab key in the ordinary mode maps to its command");
    }
    for (const auto &[key, command] : vim)
    {
        expect(tab_command_for(key, EditMode::vim) == command,
               "a tab key in the vim mode maps to its command, and control-w does nothing");
    }
}

// 行き先の表（ADR 0057 の決定 1）。期待値は Vim 9.1 の実測（probe-vimtabs-2026-09-29 の
// 節 A・B・I）。行は回数（値なし・0・1・2・3・4・9）、列は今の位置（0 始まり）、
// 値は行き先で failed は失敗（動かず・後ろの鍵は打ち切られる）。
constexpr std::size_t failed = 99;
using DestinationRow = std::pair<std::optional<std::size_t>, std::array<std::size_t, 3>>;

void verify_tab_destination_rows(nenenib::core::TabJumpDirection direction, std::size_t tab_count,
                                 const std::array<DestinationRow, 7> &rows)
{
    using nenenib::core::tab_destination;
    using nenenib::core::TabJump;
    for (const auto &[count, expected] : rows)
    {
        for (std::size_t active = 0; active < tab_count; ++active)
        {
            const auto destination = tab_destination(TabJump{direction, count}, active, tab_count);
            const std::size_t wanted = expected.at(active);
            expect(wanted == failed ? !destination.has_value()
                                    : destination == std::optional<std::size_t>{wanted},
                   "the tab destination matches the measured Vim table");
        }
    }
}

void verify_tab_destination()
{
    using nenenib::core::TabJumpDirection;
    constexpr std::size_t none = failed;
    const std::array<DestinationRow, 7> forward_three{{{std::nullopt, {1, 2, 0}},
                                                       {0, {none, none, none}},
                                                       {1, {0, 0, 0}},
                                                       {2, {1, 1, 1}},
                                                       {3, {2, 2, 2}},
                                                       {4, {none, none, none}},
                                                       {9, {none, none, none}}}};
    const std::array<DestinationRow, 7> backward_three{{{std::nullopt, {2, 0, 1}},
                                                        {0, {none, none, none}},
                                                        {1, {2, 0, 1}},
                                                        {2, {1, 2, 0}},
                                                        {3, {0, 1, 2}},
                                                        {4, {2, 0, 1}},
                                                        {9, {0, 1, 2}}}};
    const std::array<DestinationRow, 7> forward_one{{{std::nullopt, {0, none, none}},
                                                     {0, {none, none, none}},
                                                     {1, {0, none, none}},
                                                     {2, {none, none, none}},
                                                     {3, {none, none, none}},
                                                     {4, {none, none, none}},
                                                     {9, {none, none, none}}}};
    const std::array<DestinationRow, 7> backward_one{{{std::nullopt, {0, none, none}},
                                                      {0, {none, none, none}},
                                                      {1, {0, none, none}},
                                                      {2, {0, none, none}},
                                                      {3, {0, none, none}},
                                                      {4, {0, none, none}},
                                                      {9, {0, none, none}}}};
    verify_tab_destination_rows(TabJumpDirection::forward, 3, forward_three);
    verify_tab_destination_rows(TabJumpDirection::backward, 3, backward_three);
    verify_tab_destination_rows(TabJumpDirection::forward, 1, forward_one);
    verify_tab_destination_rows(TabJumpDirection::backward, 1, backward_one);
    expect(!nenenib::core::tab_destination(
                nenenib::core::TabJump{TabJumpDirection::forward, std::nullopt}, 3, 3)
                .has_value(),
           "a position outside the band has no destination");
}
// ---------------------------------------------------------------- 使った順（ADR 0058）

[[nodiscard]] bool order_is(const nenenib::core::TabRecency &recency,
                            std::initializer_list<std::size_t> expected)
{
    return std::ranges::equal(recency.order(), expected);
}

// 列が 0 から本数 - 1 までの帯の位置を 1 回ずつ含む（決定 1 の不変条件）。
[[nodiscard]] bool covers_every_tab(const nenenib::core::TabRecency &recency)
{
    const auto order = recency.order();
    std::vector<bool> seen(order.size(), false);
    for (const std::size_t tab : order)
    {
        if (tab >= seen.size() || seen.at(tab))
        {
            return false;
        }
        seen.at(tab) = true;
    }
    return !order.empty();
}

// 4 つの純関数（決定 1）。範囲の外の位置は同じ列（歩きは値なし）。
void verify_tab_recency_functions()
{
    using nenenib::core::tab_recency_closed;
    using nenenib::core::tab_recency_opened;
    using nenenib::core::tab_recency_touched;
    using nenenib::core::tab_recency_walked;
    using nenenib::core::TabRecency;
    const auto single = TabRecency::single();
    expect(order_is(single, {0}) && tab_recency_walked(single, 0, TabStep::next) == 0 &&
               tab_recency_walked(single, 0, TabStep::previous) == 0,
           "one tab walks to itself");
    expect(order_is(tab_recency_closed(single, 0), {0}) &&
               !tab_recency_walked(single, 1, TabStep::next).has_value(),
           "the last tab is not closed and a tab outside the list has no neighbour");
    const auto three = tab_recency_opened(tab_recency_opened(single, 1), 2);
    expect(order_is(three, {2, 1, 0}), "a new tab goes to the front");
    const auto shifted = tab_recency_opened(three, 0);
    expect(order_is(shifted, {0, 3, 2, 1}), "a new tab shifts the positions at and after it");
    expect(order_is(tab_recency_opened(shifted, 5), {0, 3, 2, 1}) &&
               order_is(tab_recency_opened(shifted, 4), {4, 0, 3, 2, 1}),
           "a new tab may go right of the last one but not beyond");
    const auto touched = tab_recency_touched(shifted, 2);
    expect(order_is(touched, {2, 0, 3, 1}) &&
               order_is(tab_recency_touched(touched, 4), {2, 0, 3, 1}),
           "touching moves a tab to the front and ignores a position outside the band");
    expect(order_is(tab_recency_closed(touched, 0), {1, 2, 0}) &&
               order_is(tab_recency_closed(touched, 3), {2, 0, 1}) &&
               order_is(tab_recency_closed(touched, 4), {2, 0, 3, 1}),
           "closing removes a tab and shifts the positions after it");
    expect(tab_recency_walked(touched, 2, TabStep::next) == 0 &&
               tab_recency_walked(touched, 3, TabStep::next) == 1 &&
               tab_recency_walked(touched, 1, TabStep::next) == 2,
           "next walks to the tab used before and wraps at the end");
    expect(tab_recency_walked(touched, 2, TabStep::previous) == 1 &&
               tab_recency_walked(touched, 3, TabStep::previous) == 0,
           "previous walks the other way and wraps at the front");
}

enum class RecencyStep : std::uint8_t
{
    open,
    touch,
    close
};

[[nodiscard]] nenenib::core::TabRecency stepped_recency(const nenenib::core::TabRecency &recency,
                                                        RecencyStep step, std::size_t tab)
{
    switch (step)
    {
    case RecencyStep::open:
        return nenenib::core::tab_recency_opened(recency, tab);
    case RecencyStep::touch:
        return nenenib::core::tab_recency_touched(recency, tab);
    case RecencyStep::close:
        return nenenib::core::tab_recency_closed(recency, tab);
    }
    std::unreachable();
}

// 操作を混ぜ続けても、列は開いている全部のタブを 1 回ずつ含む（決定 1）。範囲の外も混ぜる。
void verify_tab_recency_keeps_every_tab()
{
    using Row = std::pair<RecencyStep, std::size_t>;
    constexpr std::array<Row, 22> rows{{
        {RecencyStep::open, 1},  {RecencyStep::open, 0},  {RecencyStep::touch, 2},
        {RecencyStep::open, 3},  {RecencyStep::close, 1}, {RecencyStep::open, 2},
        {RecencyStep::touch, 0}, {RecencyStep::close, 3}, {RecencyStep::open, 1},
        {RecencyStep::touch, 3}, {RecencyStep::open, 4},  {RecencyStep::close, 0},
        {RecencyStep::close, 9}, {RecencyStep::touch, 7}, {RecencyStep::open, 2},
        {RecencyStep::close, 4}, {RecencyStep::touch, 1}, {RecencyStep::open, 5},
        {RecencyStep::close, 2}, {RecencyStep::open, 0},  {RecencyStep::touch, 4},
        {RecencyStep::close, 1},
    }};
    auto recency = nenenib::core::TabRecency::single();
    bool whole = true;
    for (const auto &[step, tab] : rows)
    {
        recency = stepped_recency(recency, step, tab);
        whole = whole && covers_every_tab(recency);
    }
    expect(whole && order_is(recency, {0, 1, 2}),
           "every mix of opening, touching and closing keeps each tab once");
}

// 使った順に並べた本文の 1 行目（いちばん最近から）。歩いていないときに、列の上を next で端まで
// 歩いて読み、previous で同じ数だけ戻って確定する。戻った先は先頭なので、列は変わらない。
[[nodiscard]] std::string recent_bodies(EditorController &controller)
{
    const std::size_t count = controller.frame().tabs.size();
    std::string bodies = first_line(controller.frame());
    for (std::size_t step = 1; step < count; ++step)
    {
        bodies += first_line(controller.apply(WalkRecentTab{TabStep::next}));
    }
    for (std::size_t step = 1; step < count; ++step)
    {
        applied(controller, WalkRecentTab{TabStep::previous});
    }
    applied(controller, SettleRecentTab{});
    return bodies;
}

// 押して離すと直前の 2 つを行き来し、押したまま続けると 3 つ目以降へ届く（決定 2・3）。
void verify_walk_recent_tabs()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    expect(recent_bodies(controller) == "dcba", "each new tab goes to the front of the order");
    const auto walked = controller.apply(WalkRecentTab{TabStep::next});
    expect(first_line(walked) == "c" && walked.active_tab == 2 && controller.tab_walking(),
           "one walk goes to the tab used before");
    applied(controller, SettleRecentTab{});
    expect(!controller.tab_walking() && recent_bodies(controller) == "cdba",
           "releasing Ctrl moves the reached tab to the front");
    applied(controller, WalkRecentTab{TabStep::next});
    applied(controller, SettleRecentTab{});
    expect(first_line(controller.frame()) == "d" && recent_bodies(controller) == "dcba",
           "press and release again goes back to the tab before");
    applied(controller, WalkRecentTab{TabStep::next});
    const auto held = controller.apply(WalkRecentTab{TabStep::next});
    expect(first_line(held) == "b" && held.active_tab == 1,
           "the second walk with Ctrl held reaches the third tab in the order");
    applied(controller, SettleRecentTab{});
    expect(recent_bodies(controller) == "bdca", "settling after two walks moves only that tab");
    for (std::size_t step = 0; step < 4; ++step)
    {
        applied(controller, WalkRecentTab{TabStep::next});
    }
    applied(controller, SettleRecentTab{});
    expect(first_line(controller.frame()) == "b" && recent_bodies(controller) == "bdca",
           "four walks over four tabs wrap back to the start");
    const auto reverse = controller.apply(WalkRecentTab{TabStep::previous});
    expect(first_line(reverse) == "a", "previous walks to the least recently used tab");
    applied(controller, SettleRecentTab{});
    expect(recent_bodies(controller) == "abdc", "and settling moves it to the front");
    expect(first_line(controller.apply(WalkRecentTab{TabStep::next})) == "b" &&
               first_line(controller.apply(WalkRecentTab{TabStep::previous})) == "a",
           "next and previous walk back and forth over the same frozen order");
    applied(controller, SettleRecentTab{});
}

// 歩きの途中のクリックと gt は、先に歩きを確定してから（決定 4）、着いたタブを先頭へ動かす
// （決定 2）。gt は帯の位置の順。
void verify_walk_interrupted()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, WalkRecentTab{TabStep::next});
    const auto clicked = controller.apply(SwitchTab{0});
    expect(first_line(clicked) == "a" && !controller.tab_walking() &&
               recent_bodies(controller) == "acdb",
           "a click during the walk settles it and moves the clicked tab to the front");
    applied(controller, WalkRecentTab{TabStep::next});
    applied(controller, SwitchTab{2});
    expect(!controller.tab_walking() && recent_bodies(controller) == "cadb",
           "a click on the tab reached by the walk settles there");
    applied(controller, SelectEditMode{EditMode::vim});
    applied(controller, WalkRecentTab{TabStep::next});
    vim_normal(controller, "gt");
    expect(first_line(controller.frame()) == "b" && !controller.tab_walking() &&
               recent_bodies(controller) == "bacd",
           "gt during the walk goes by the band from the reached tab and ends the walk");
    vim_normal(controller, "gT");
    expect(first_line(controller.frame()) == "a" && recent_bodies(controller) == "abcd",
           "gT keeps the band order and moves the tab it reaches to the front");
}

// 新しいタブは先頭、閉じた後は新しくアクティブになったタブが先頭（決定 2）。
void verify_recent_after_open_and_close()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, SwitchTab{1});
    applied(controller, NewTab{});
    applied(controller, InsertText{"n"});
    expect(recent_bodies(controller) == "nbdca", "a new tab right of the active one is the newest");
    const auto closed = controller.apply(CloseTab{2});
    expect(first_line(closed) == "c" && recent_bodies(controller) == "cbda",
           "closing the active tab moves its right neighbour to the front");
    applied(controller, CloseTab{0});
    expect(first_line(controller.frame()) == "c" && recent_bodies(controller) == "cbd",
           "closing a parked tab keeps the order of the others");
    applied(controller, WalkRecentTab{TabStep::next});
    applied(controller, CloseTab{2});
    expect(!controller.tab_walking() && first_line(controller.frame()) == "b" &&
               recent_bodies(controller) == "bc",
           "closing during the walk ends it at the reached tab");
    applied(controller, WalkRecentTab{TabStep::next});
    applied(controller, NewTab{});
    applied(controller, InsertText{"m"});
    expect(!controller.tab_walking() && recent_bodies(controller) == "mcb",
           "a new tab during the walk settles the reached tab before it");
}

// 起動引数のファイルは最後に開けたものが先頭で、前に開いたものほど後ろ（決定 2）。
void verify_recent_after_startup()
{
    ScriptedFiles files;
    hold_three(files);
    ScriptedAppearance appearance{Reading{Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    ScriptedSession session;
    EditorController controller(
        EditorPorts{appearance, clipboard, files, code_pages, settings, themes, session},
        {open_at("C:\\work\\a.txt"), open_at("C:\\work\\b.txt"), open_at("C:\\work\\c.txt")});
    applied(controller, VisibleLines{10});
    expect(recent_bodies(controller) == "cba", "three arguments leave the last one most recent");
}

[[nodiscard]] std::string message_text(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

// 1 本のときの歩きは同じ位置。歩いていないときの確定は何も変えず、Vim の報せも消さない
// （決定 1・3）。
void verify_settle_without_walk()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    const auto alone = controller.apply(WalkRecentTab{TabStep::next});
    expect(alone.active_tab == 0 && first_line(alone) == "a" && controller.tab_walking(),
           "walking with one tab stays on it");
    applied(controller, SettleRecentTab{});
    expect(!controller.tab_walking(), "settling ends the walk");
    open_four_tabs(controller);
    applied(controller, SelectEditMode{EditMode::vim});
    const auto failed_frame = run_ex(controller, "tabnext 9");
    const auto settled = controller.apply(SettleRecentTab{});
    expect(settled.active_tab == failed_frame.active_tab && settled.tabs.size() == 4 &&
               first_line(settled) == first_line(failed_frame) &&
               settled.tab_scroll == failed_frame.tab_scroll &&
               message_text(settled) == message_text(failed_frame) &&
               settled.command_message.has_value() && !controller.tab_walking(),
           "settling without a walk changes nothing and keeps the Vim message");
    expect(recent_bodies(controller) == "dcbaa", "and the order stays as it was");
}

// 歩きを続けない意図は、controller が写す前に歩きを確定する（決定 4）。窓を経ずに controller を
// 直に叩いても確定は漏れない。帯の上のマウスと窓の行数は歩きを続ける。
void verify_walk_settles_before_other_intents()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, WalkRecentTab{TabStep::next});
    applied(controller, PointTitleBar{TitleBarTarget{TitleBarHit::tab, 1}});
    applied(controller, VisibleLines{12});
    const auto third = controller.apply(WalkRecentTab{TabStep::next});
    expect(first_line(third) == "b" && controller.tab_walking(),
           "hovering the band and resizing during the walk keep it going to the third tab");
    applied(controller, SettleRecentTab{});
    applied(controller, WalkRecentTab{TabStep::next});
    const auto typed = controller.apply(InsertText{"x"});
    expect(first_line(typed) == "dx" && !controller.tab_walking(),
           "typing during the walk settles it and edits the reached tab");
    expect(first_line(controller.apply(WalkRecentTab{TabStep::next})) == "b",
           "the next walk after typing goes back to the tab the walk left");
    applied(controller, SettleRecentTab{});
    expect(recent_bodies(controller) == "bdxca", "and the order is the settled one");
    applied(controller, SelectEditMode{EditMode::vim});
    applied(controller, WalkRecentTab{TabStep::next});
    vim_replay(controller, "0");
    expect(!controller.tab_walking() && first_line(controller.frame()) == "dx" &&
               recent_bodies(controller) == "dxbca",
           "a Vim key during the walk settles it");
    applied(controller, WalkRecentTab{TabStep::next});
    vim_normal(controller, "0");
    expect(!controller.tab_walking() && recent_bodies(controller) == "bdxca",
           "Vim keys given as a list settle the walk too");
}

// 歩いていないときは、切り替え・開く・閉じる以外の意図で使った順は変わらない（決定 2・4）。
void verify_order_kept_without_walk()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, SwitchTab{1});
    applied(controller, InsertText{"y"});
    applied(controller, PointTitleBar{TitleBarTarget{TitleBarHit::tab, 3}});
    applied(controller, VisibleLines{12});
    applied(controller, SettleRecentTab{});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "0");
    vim_normal(controller, "l");
    expect(!controller.tab_walking() && recent_bodies(controller) == "bydca",
           "intents other than switching, opening and closing keep the order");
}

// Vim の 3 本のタブ（本文 alpha / bravo / charlie・キャレットはどれも行頭）。アクティブは先頭。
// 本文は通常モードで打つので、`.` の直前の変更は空のまま。
void open_three_vim_tabs(Editing &editing)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"alpha"});
    for (const std::string_view body : {"bravo", "charlie"})
    {
        applied(controller, NewTab{});
        applied(controller, InsertText{std::string(body)});
    }
    applied(controller, SelectEditMode{EditMode::vim});
    for (const std::size_t tab : {0U, 1U, 2U})
    {
        applied(controller, SwitchTab{tab});
        vim_replay(controller, "0");
    }
    applied(controller, SwitchTab{0});
}

[[nodiscard]] std::size_t active_after(EditorController &controller, std::string_view keys)
{
    vim_normal(controller, keys);
    return controller.frame().active_tab;
}

// `gt` `gT` は折り返し、`{N}gt` は絶対の番号、`{N}gT` は N 個ぶん戻る（ADR 0057 の決定 2・実測
// A）。
void verify_vim_tab_keys()
{
    Editing editing;
    open_three_vim_tabs(editing);
    EditorController &controller = editing.controller();
    expect(active_after(controller, "gt") == 1 && active_after(controller, "gt") == 2 &&
               active_after(controller, "gt") == 0,
           "gt moves to the next tab and wraps from the last to the first");
    expect(active_after(controller, "gT") == 2 && active_after(controller, "gT") == 1,
           "gT moves to the previous tab and wraps from the first to the last");
    expect(active_after(controller, "3gt") == 2 && active_after(controller, "1gt") == 0 &&
               active_after(controller, "2gt") == 1,
           "a count before gt names the tab by its number");
    expect(active_after(controller, "2gT") == 2 && active_after(controller, "4gT") == 1 &&
               active_after(controller, "3gT") == 1,
           "a count before gT goes back that many tabs and wraps");
    vim_normal(controller, "1gt");
    const auto same = active_after(controller, "1gtx");
    expect(same == 0 && vim_body(controller.frame()) == "lpha",
           "1gt on the first tab succeeds and the keys after it run");
    const auto outside = active_after(controller, "9gtx");
    expect(outside == 0 && vim_body(controller.frame()) == "lpha",
           "9gt with three tabs fails, stays and drops the keys after it");
    vim_normal(controller, "$");
    const auto zero = active_after(controller, "0gt");
    applied(controller, SwitchTab{0});
    expect(zero == 1 && caret_at(controller.frame(), 1, 1),
           "0gt is the 0 motion followed by gt, not a count of zero");
}

// オペレータの後ろの gt gT は打ち消して失敗し、VISUAL の gt は切り替えて NORMAL へ（実測 G）。
void verify_vim_tab_keys_after_operator()
{
    Editing editing;
    open_three_vim_tabs(editing);
    EditorController &controller = editing.controller();
    for (const std::string_view keys : {"dgtx", "ygtx", "cgtx", "d2gtx", "dgTx"})
    {
        const auto active = active_after(controller, keys);
        expect(active == 0 && vim_body(controller.frame()) == "alpha" &&
                   controller.vim_state().mode == VimMode::normal &&
                   !controller.vim_state().pending.has_value(),
               "an operator before gt is cancelled and the keys after it are dropped");
    }
    vim_replay(controller, "vl");
    const auto visual = active_after(controller, "gt");
    expect(visual == 1 && controller.vim_state().mode == VimMode::normal,
           "gt in VISUAL switches the tab and ends VISUAL");
    const auto back = controller.apply(SwitchTab{0});
    expect(back.lines.at(0).selection.presence == nenenib::core::SelectionPresence::absent,
           "the tab left from VISUAL keeps no selection");
}

// gt は `.` の対象ではなく、マクロに録った gt は動く（実測 H）。範囲の外は再生を打ち切る。
void verify_vim_tab_keys_repeat()
{
    using nenenib::application::StoreVimRegister;
    using nenenib::core::VimRegister;
    using nenenib::core::VimRegisterKind;
    Editing editing;
    open_three_vim_tabs(editing);
    EditorController &controller = editing.controller();
    const auto dotted = active_after(controller, "gt.");
    expect(dotted == 1 && vim_body(controller.frame()) == "bravo" &&
               !controller.vim_state().last_change.has_value(),
           "the dot after gt does nothing");
    const auto store = [&controller](std::string_view keys)
    {
        applied(
            controller,
            StoreVimRegister{'a', VimRegister{nenenib::core::vim_register_text(vim_keys_of(keys)),
                                              VimRegisterKind::characters}});
    };
    store("gt");
    expect(active_after(controller, "@a") == 2 && active_after(controller, "@a") == 0,
           "a macro holding gt moves one tab each time");
    applied(controller, SwitchTab{2});
    expect(active_after(controller, "2@a") == 1, "a count before the macro repeats gt");
    applied(controller, SwitchTab{0});
    store("9gtx");
    vim_replay(controller, "@a");
    expect(controller.frame().active_tab == 0 && vim_body(controller.frame()) == "alpha",
           "a failing 9gt in a macro drops the keys after it");
}

// 再生の途中の切り替え（ADR 0057 の決定 3）。出ていく文書の単位を閉じ、残りの鍵は入った文書へ。
void verify_vim_tab_switch_in_macro()
{
    using nenenib::application::StoreVimRegister;
    using nenenib::core::VimRegister;
    using nenenib::core::VimRegisterKind;
    Editing editing;
    open_three_vim_tabs(editing);
    EditorController &controller = editing.controller();
    applied(controller,
            StoreVimRegister{'a', VimRegister{nenenib::core::vim_register_text(vim_keys_of("xgtx")),
                                              VimRegisterKind::characters}});
    vim_replay(controller, "@a");
    const auto entered = controller.frame();
    expect(entered.active_tab == 1 && vim_body(entered) == "ravo",
           "the keys after gt in a macro run in the entered tab");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "bravo", "one u undoes the entered tab's part");
    applied(controller, SwitchTab{0});
    expect(vim_body(controller.frame()) == "lpha", "the left tab keeps its own part");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "alpha", "one u undoes the left tab's part");
}
[[nodiscard]] std::size_t active_after_ex(EditorController &controller, std::string text)
{
    return run_ex(controller, std::move(text)).active_tab;
}

[[nodiscard]] bool message_is(const EditorFrame &frame, std::string_view text)
{
    return frame.command_message.has_value() && frame.command_message.value().text() == text;
}

// `:tabnext` `:tabprevious` は gt / gT と同じ行き先で、範囲の外は Vim の文言（決定 4・5・実測 B）。
void verify_ex_tab_switch()
{
    Editing editing;
    open_three_vim_tabs(editing);
    EditorController &controller = editing.controller();
    expect(active_after_ex(controller, "tabnext") == 1 &&
               active_after_ex(controller, "tabn") == 2 &&
               active_after_ex(controller, "tabne") == 0,
           ":tabnext moves to the next tab and wraps");
    expect(active_after_ex(controller, "tabprevious") == 2 &&
               active_after_ex(controller, "tabN") == 1 && active_after_ex(controller, "tabp") == 0,
           ":tabprevious and :tabNext move to the previous tab and wrap");
    expect(active_after_ex(controller, "tabnext 3") == 2 &&
               active_after_ex(controller, "tabnext 1") == 0 &&
               active_after_ex(controller, "tabprevious 2") == 1 &&
               active_after_ex(controller, "tabprevious 4") == 0,
           ":tabnext N names the tab and :tabprevious N goes back N tabs");
    for (const auto &[text, message] : std::array<std::pair<std::string_view, std::string_view>, 3>{
             {{"tabnext 4", "E475: Invalid argument: 4"},
              {"tabnext 0", "E475: Invalid argument: 0"},
              {"tabprevious 0", "E475: Invalid argument: 0"}}})
    {
        const auto failed_frame = run_ex(controller, std::string(text));
        expect(failed_frame.active_tab == 0 && message_is(failed_frame, message),
               "a number outside the band stays and shows the measured Vim message");
    }
    const auto unsupported = run_ex(controller, "tabnext +1");
    expect(unsupported.active_tab == 0 && message_is(unsupported, "Not supported: tabnext +1"),
           "a relative argument is not supported yet");
    const auto moved = run_ex(controller, "tabnext 2");
    expect(moved.active_tab == 1 && !moved.command_message.has_value() &&
               !moved.command_line.has_value(),
           "a switch by Ex shows no message and closes the Ex line");
}

// `:tabnew` は今の右隣、`:tabclose` は閉じたい位置を 1 意図だけ載せて状態を変えない（決定 5・6）。
void verify_ex_tab_open_and_close()
{
    Editing editing;
    open_three_vim_tabs(editing);
    EditorController &controller = editing.controller();
    const auto opened = run_ex(controller, "tabnew");
    expect(opened.tabs.size() == 4 && opened.active_tab == 1 && vim_body(opened).empty() &&
               first_line(controller.apply(SwitchTab{2})) == "bravo",
           ":tabnew adds an empty tab right of the active one and moves there");
    const auto closing = run_ex(controller, "tabclose");
    expect(closing.close_request == std::optional<std::size_t>{2} && closing.tabs.size() == 4 &&
               closing.active_tab == 2 && first_line(closing) == "bravo" && !closing.closing,
           ":tabclose asks to close the active tab and leaves the state alone");
    expect(!controller.apply(VisibleLines{10}).close_request.has_value(),
           "the close request lasts one intent");
    expect(!run_ex(controller, "tabc").closing && controller.frame().tabs.size() == 4,
           "the abbreviation asks the same way");
}

// 一覧の行が帯と同じ順と題名で、実行が tabnext N で、場所がフォルダ（無題は無し）か。
[[nodiscard]] bool rows_follow_band(const EditorFrame &listed)
{
    if (!listed.command_palette.has_value())
    {
        return false;
    }
    const auto &choices = listed.command_palette.value().choices;
    bool same = choices.size() == listed.tabs.size();
    for (std::size_t index = 0; same && index < choices.size(); ++index)
    {
        const auto &choice = choices.at(index);
        const auto folder = nenenib::core::tab_folder_for(listed.tabs.at(index).path);
        const auto &detail = choice.detail;
        same = choice.label.text() == listed.tabs.at(index).title.text() &&
               choice.command == "tabnext " + std::to_string(index + 1) &&
               choice.kind == nenenib::core::CommandChoiceKind::execute &&
               choice.origin == std::optional{nenenib::core::PaletteOrigin::tab} &&
               detail.has_value() == folder.has_value() &&
               (!detail.has_value() || !folder.has_value() ||
                detail.value().text() == folder.value().text());
    }
    return same && choices.size() == 3 && choices.at(2).label.text() == "● 無題" &&
           choices.at(0).detail.has_value() && !choices.at(1).detail.has_value();
}

// 一覧（決定 7）。帯の順・アクティブの行・題名は帯と同じ・場所はフォルダ・実行は tabnext N。
void verify_tab_list_rows()
{
    Editing editing;
    open_vim_document(editing, "alpha");
    EditorController &controller = editing.controller();
    applied(controller, NewTab{});
    applied(controller, NewTab{});
    vim_replay(controller, "ix<Esc>");
    applied(controller, SwitchTab{1});
    const auto listed = controller.apply(nenenib::application::OpenTabList{});
    const auto &palette = listed.command_palette;
    const auto &line = listed.command_line;
    expect(palette.has_value() && line.has_value() && palette.value().choices.size() == 3 &&
               palette.value().selected == 1 && line.value().text == "#" &&
               line.value().completions.empty(),
           "it lists every tab with the active one selected and the tabs mark as input");
    expect(rows_follow_band(listed), "each row is the band title with its mark and runs tabnext N");
    const auto chosen = controller.apply(nenenib::application::ActivateCommandChoice{0});
    expect(chosen.active_tab == 0 && !chosen.command_palette.has_value() &&
               !chosen.command_line.has_value(),
           "running a row switches to that tab and closes the list");
}

[[nodiscard]] std::vector<std::string> listed_commands(const EditorFrame &frame)
{
    std::vector<std::string> commands;
    if (frame.command_palette.has_value())
    {
        for (const auto &choice : frame.command_palette.value().choices)
        {
            commands.push_back(choice.command);
        }
    }
    return commands;
}

// `:tabs` と OpenTabList と Ctrl+P の候補 `tabs` と Ctrl+P の空の入力は同じ列を開き、開いている間は
// 閉じる（ADR 0060 の決定 6）。
void verify_tab_list_entries()
{
    using nenenib::application::CommandText;
    using nenenib::application::OpenCommandPalette;
    using nenenib::application::OpenTabList;
    using nenenib::application::SubmitCommand;
    Editing editing;
    open_vim_document(editing, "alpha");
    EditorController &controller = editing.controller();
    applied(controller, NewTab{});
    applied(controller, NewTab{});
    const std::vector<std::string> all{"tabnext 1", "tabnext 2", "tabnext 3"};
    expect(listed_commands(controller.apply(OpenTabList{})) == all, "OpenTabList opens the list");
    expect(!controller.apply(OpenTabList{}).command_palette.has_value(),
           "OpenTabList closes the open list");
    expect(listed_commands(run_ex(controller, "tabs")) == all, ":tabs opens the same list");
    expect(!controller.apply(OpenCommandPalette{}).command_palette.has_value(),
           "Ctrl+P closes the open list");
    expect(listed_commands(controller.apply(OpenCommandPalette{})) == all,
           "Ctrl+P opens the same entries with an empty input");
    applied(controller, CommandText{":tabs"});
    expect(listed_commands(controller.apply(SubmitCommand{})) == all,
           "the Ctrl+P candidate tabs opens the same list");
    applied(controller, CommandText{"NOTE"});
    const auto filtered = controller.frame();
    expect(listed_commands(filtered) == std::vector<std::string>{"tabnext 1"},
           "typing filters the rows by title, ignoring case");
    const auto ran = controller.apply(SubmitCommand{});
    expect(ran.active_tab == 0 && !ran.command_palette.has_value(), "Enter runs the selected row");
    applied(controller, OpenTabList{});
    const auto up = controller.apply(
        nenenib::application::EditCommand{nenenib::core::CommandEdit::complete_previous});
    expect(up.command_palette.has_value() && up.command_palette.value().selected == 2,
           "moving up from the first row wraps to the last");
    applied(controller, nenenib::application::CancelCommand{});
    applied(controller, SelectEditMode{EditMode::ordinary});
    applied(controller, ComposeText{composed_of("あ", {}, 0)});
    expect(!controller.apply(OpenTabList{}).command_palette.has_value(),
           "the list does not open during a composition");
}
} // namespace

void verify_tabs_contracts()
{
    verify_new_tab();
    verify_switch_keeps_each_document();
    verify_undo_per_document();
    verify_close_moves_active();
    verify_close_right_of_active();
    verify_last_tab_closes_window();
    verify_vim_switch_discards_pending();
    verify_vim_switch_leaves_insert();
    verify_vim_switched_document();
    verify_switch_closes_input();
    verify_parked_references();
    verify_open_same_file();
    verify_open_same_file_by_port();
    verify_open_into_blank_untitled();
    verify_open_right_of_active();
    verify_open_failure_adds_no_tab();
    verify_startup_opens_every_file();
    verify_unsaved_tabs_in_band_order();
    verify_band_scroll();
    verify_band_hover();
    verify_band_pointer();
    verify_band_release();
    verify_band_release_after_scroll();
    verify_band_input_from_state();
    verify_tab_keys();
    verify_tab_destination();
    verify_tab_recency_functions();
    verify_tab_recency_keeps_every_tab();
    verify_walk_recent_tabs();
    verify_walk_interrupted();
    verify_recent_after_open_and_close();
    verify_recent_after_startup();
    verify_settle_without_walk();
    verify_walk_settles_before_other_intents();
    verify_order_kept_without_walk();
    verify_vim_tab_keys();
    verify_vim_tab_keys_after_operator();
    verify_vim_tab_keys_repeat();
    verify_vim_tab_switch_in_macro();
    verify_ex_tab_switch();
    verify_ex_tab_open_and_close();
    verify_tab_list_rows();
    verify_tab_list_entries();
}

void verify_tabs_scope()
{
    verify_tabs_contracts();
}
} // namespace nenenib::tests
