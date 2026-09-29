// scope `--tabs` の単体テスト（ADR 0042 決定 2・ADR 0056）。状態・切り替え・閉じる（決定 1〜4・
// 6・7）と、開く（決定 5）と起動（決定 13）。
#include "Appearance.hpp"
#include "CloseTab.hpp"
#include "ComposeText.hpp"
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
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "ScrollLines.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionPresence.hpp"
#include "StepTab.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimMode.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
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
using nenenib::application::OpenDocument;
using nenenib::application::ScrollLines;
using nenenib::application::SelectEditMode;
using nenenib::application::StepTab;
using nenenib::application::SwitchTab;
using nenenib::application::VisibleLines;
using nenenib::core::Appearance;
using nenenib::core::EditMode;
using nenenib::core::FilePath;
using nenenib::core::HistoryDirection;
using nenenib::core::SaveState;
using nenenib::core::TabStep;
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

// 帯の位置の順で隣へ。端は折り返す（決定 3）。
void verify_step_wraps()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.apply(StepTab{TabStep::next}).active_tab == 0 &&
               controller.frame().tabs.size() == 1,
           "stepping with a single tab stays on it");
    open_four_tabs(controller);
    const auto wrapped = controller.apply(StepTab{TabStep::next});
    expect(wrapped.active_tab == 0 && first_line(wrapped) == "a",
           "next from the right end wraps to the left end");
    const auto back = controller.apply(StepTab{TabStep::previous});
    expect(back.active_tab == 3 && first_line(back) == "d",
           "previous from the left end wraps to the right end");
    const auto left = controller.apply(StepTab{TabStep::previous});
    expect(left.active_tab == 2 && first_line(left) == "c", "previous moves one tab left");
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
    expect(!controller.apply(StepTab{TabStep::next}).command_line.has_value(),
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
    const EditorController controller(
        EditorPorts{appearance, clipboard, files, code_pages, settings, themes}, initial);
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
} // namespace

void verify_tabs_contracts()
{
    verify_new_tab();
    verify_switch_keeps_each_document();
    verify_undo_per_document();
    verify_step_wraps();
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
}

void verify_tabs_scope()
{
    verify_tabs_contracts();
}
} // namespace nenenib::tests
