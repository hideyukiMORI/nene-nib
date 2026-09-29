// scope `--session` の単体テスト（ADR 0042 決定 2・ADR 0059 の決定 1〜3）。状態から前回のタブの
// 一覧を作る読み取りと、窓が閉じていくときの意図 EndSession。
#include "CloseTab.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EndSession.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSession.hpp"
#include "ScrollLines.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionFailure.hpp"
#include "SettleRecentTab.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::application::CloseTab;
using nenenib::application::EditorFrame;
using nenenib::application::EndSession;
using nenenib::application::InsertText;
using nenenib::application::NewTab;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::ScrollLines;
using nenenib::application::SelectEditMode;
using nenenib::application::Session;
using nenenib::application::SessionEnd;
using nenenib::application::SessionFailure;
using nenenib::application::SettleRecentTab;
using nenenib::application::SwitchTab;
using nenenib::application::VisibleLines;
using nenenib::application::WalkRecentTab;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::FilePath;
using nenenib::core::LineNumber;
using nenenib::core::SelectionAnchoring;
using nenenib::core::TabStep;
using nenenib::core::TextPosition;

[[nodiscard]] OpenDocument open_at(std::string_view text)
{
    auto parsed = FilePath::parse(text);
    expect(parsed.has_value(), "the session contract path parses");
    return OpenDocument{std::move(parsed).value()};
}

// a.txt b.txt c.txt はどれも 6 行。1 行目がファイルの名前なので、どのタブかを本文で読める。
void hold_three(ScriptedFiles &files)
{
    for (const std::string_view name : {"a", "b", "c"})
    {
        files.hold_at("C:\\work\\" + std::string(name) + ".txt",
                      Bytes{std::string(name) + "\nl2\nl3\nl4\nl5\nl6"});
    }
}

void open_files(Editing &editing, std::string_view names)
{
    for (const char name : names)
    {
        applied(editing.controller(), open_at("C:\\work\\" + std::string(1, name) + ".txt"));
    }
}

// 一覧を「名前@行:桁/先頭の行#順位」の並びと「>位置」で読む。
[[nodiscard]] std::string described(const Session &session)
{
    std::string text;
    for (const auto &tab : session.tabs)
    {
        const FilePath &path = tab.path;
        text += std::string(path.file_name()) + "@" + std::to_string(tab.caret.line.value) + ":" +
                std::to_string(tab.caret.column.value) + "/" +
                std::to_string(tab.first_visible.value) + "#" + std::to_string(tab.recency) + " ";
    }
    return text + ">" + std::to_string(session.active);
}

[[nodiscard]] std::string written(Editing &editing)
{
    const auto &value = editing.session().written();
    return value.has_value() ? described(value.value()) : std::string("none");
}

[[nodiscard]] Editing &closed_window(Editing &editing)
{
    applied(editing.controller(), EndSession{SessionEnd::window_closed});
    return editing;
}

// 無題だけの帯は空の一覧。ファイル 1 本はその 1 本（決定 1・D9）。
void verify_untitled_and_single()
{
    Editing untitled;
    applied(untitled.controller(), VisibleLines{3});
    applied(untitled.controller(), InsertText{"x"});
    applied(untitled.controller(), NewTab{});
    expect(written(closed_window(untitled)) == ">0" && untitled.session().writes() == 1,
           "untitled tabs only write an empty list");
    Editing single;
    hold_three(single.files());
    applied(single.controller(), VisibleLines{3});
    open_files(single, "a");
    expect(written(closed_window(single)) == "a.txt@1:1/1#0 >0",
           "one file writes that file at its caret and first line");
}

// 無題は一覧に入らず、アクティブが無題なら右隣（無ければ左隣）のファイル（決定 1）。
void verify_mixed_band()
{
    Editing middle;
    hold_three(middle.files());
    applied(middle.controller(), VisibleLines{3});
    open_files(middle, "abc");
    applied(middle.controller(), SwitchTab{0});
    applied(middle.controller(), NewTab{});
    expect(written(closed_window(middle)) == "a.txt@1:1/1#0 b.txt@1:1/1#2 c.txt@1:1/1#1 >1",
           "an active untitled tab between files points at its right neighbour");
    applied(middle.controller(), SwitchTab{3});
    expect(written(closed_window(middle)) == "a.txt@1:1/1#1 b.txt@1:1/1#2 c.txt@1:1/1#0 >2",
           "an active file is found past an untitled tab left of it");
    Editing last;
    hold_three(last.files());
    applied(last.controller(), VisibleLines{3});
    open_files(last, "ab");
    applied(last.controller(), NewTab{});
    expect(written(closed_window(last)) == "a.txt@1:1/1#1 b.txt@1:1/1#0 >1",
           "an active untitled tab at the right end points at its left neighbour");
    Editing first;
    hold_three(first.files());
    applied(first.controller(), VisibleLines{3});
    applied(first.controller(), InsertText{"x"});
    open_files(first, "a");
    applied(first.controller(), SwitchTab{0});
    expect(written(closed_window(first)) == "a.txt@1:1/1#0 >0",
           "an active untitled tab at the left end points at its right neighbour");
}

// 脇に置いたタブのカーソルと画面の位置は束から、アクティブは今の欄から。未保存の変更がある
// ファイルも入る（決定 1）。
void verify_positions_and_unsaved()
{
    Editing editing;
    hold_three(editing.files());
    applied(editing.controller(), VisibleLines{3});
    open_files(editing, "a");
    applied(editing.controller(),
            PlaceCaret{TextPosition{LineNumber{2}, Column{2}}, SelectionAnchoring::collapse});
    applied(editing.controller(), ScrollLines{2});
    open_files(editing, "b");
    applied(editing.controller(),
            PlaceCaret{TextPosition{LineNumber{3}, Column{1}}, SelectionAnchoring::collapse});
    const EditorFrame edited = editing.controller().apply(InsertText{"yy"});
    expect(edited.document.title.text() == "● b.txt", "the active file has unsaved changes");
    expect(written(closed_window(editing)) == "a.txt@2:2/3#1 b.txt@3:3/1#0 >1",
           "a parked tab keeps its caret and first line and an unsaved file is listed");
    applied(editing.controller(), SwitchTab{0});
    expect(written(closed_window(editing)) == "a.txt@2:2/3#0 b.txt@3:3/1#1 >0",
           "after switching the positions come from the other side");
}

// 閉じたタブは入らない。使った順は一覧に入るタブだけで詰め直す（決定 1）。
void verify_after_close()
{
    Editing editing;
    hold_three(editing.files());
    applied(editing.controller(), VisibleLines{3});
    open_files(editing, "abc");
    applied(editing.controller(), CloseTab{1});
    expect(written(closed_window(editing)) == "a.txt@1:1/1#1 c.txt@1:1/1#0 >1",
           "a closed tab is not listed and the order closes up");
}

// 最後の 1 本を閉じたときは空の一覧。書くのは意図 1 つに 1 回（決定 3）。
void verify_last_tab_closed()
{
    Editing editing;
    hold_three(editing.files());
    applied(editing.controller(), VisibleLines{3});
    open_files(editing, "ab");
    applied(editing.controller(), EndSession{SessionEnd::last_tab_closed});
    expect(written(editing) == ">0" && editing.session().writes() == 1,
           "closing the last tab writes an empty list once");
}

// 普通の編集・切り替え・タブの開閉・歩きでは書かない（決定 3）。
void verify_no_write_on_ordinary_intents()
{
    Editing editing;
    hold_three(editing.files());
    auto &controller = editing.controller();
    applied(controller, VisibleLines{3});
    open_files(editing, "abc");
    applied(controller, InsertText{"z"});
    applied(controller, SwitchTab{0});
    applied(controller, NewTab{});
    applied(controller, CloseTab{1});
    applied(controller, WalkRecentTab{TabStep::next});
    applied(controller, SettleRecentTab{});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "jx");
    expect(editing.session().writes() == 0 && !editing.session().written().has_value(),
           "editing, switching, opening and closing tabs never write the list");
}

// 書けなくても状態も表示値も変わらず、何も出さない。Vim の報せも消さない（決定 3）。
void verify_write_failure_changes_nothing()
{
    Editing editing;
    hold_three(editing.files());
    auto &controller = editing.controller();
    applied(controller, VisibleLines{3});
    open_files(editing, "ab");
    applied(controller, SelectEditMode{EditMode::vim});
    const EditorFrame before = run_ex(controller, "tabnext 9");
    editing.session().fail(SessionFailure::unwritable);
    const EditorFrame after = controller.apply(EndSession{SessionEnd::window_closed});
    const auto message = [](const EditorFrame &frame)
    {
        return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                                 : std::string{};
    };
    expect(editing.session().writes() == 1 && !editing.session().written().has_value(),
           "a failed write is tried once");
    expect(after.tabs.size() == before.tabs.size() && after.active_tab == before.active_tab &&
               vim_body(after) == vim_body(before) &&
               after.document.title.text() == before.document.title.text() &&
               after.first_visible == before.first_visible && after.caret == before.caret &&
               !after.document.last_failure.has_value() && message(after) == message(before) &&
               after.command_message.has_value(),
           "a failed write changes neither the state nor the frame and keeps the Vim message");
}

// 歩いている途中の EndSession は、確定した後の使った順を書く（決定 3・ADR 0058 の決定 4）。
void verify_walk_settles_before_writing()
{
    Editing editing;
    hold_three(editing.files());
    auto &controller = editing.controller();
    applied(controller, VisibleLines{3});
    open_files(editing, "abc");
    applied(controller, WalkRecentTab{TabStep::next});
    expect(controller.tab_walking(), "the walk is under way");
    expect(written(closed_window(editing)) == "a.txt@1:1/1#2 b.txt@1:1/1#0 c.txt@1:1/1#1 >1" &&
               !controller.tab_walking(),
           "ending the session settles the walk before the list is written");
}
} // namespace

void verify_session_contracts()
{
    verify_untitled_and_single();
    verify_mixed_band();
    verify_positions_and_unsaved();
    verify_after_close();
    verify_last_tab_closed();
    verify_no_write_on_ordinary_intents();
    verify_write_failure_changes_nothing();
    verify_walk_settles_before_writing();
}

void verify_session_scope()
{
    verify_session_contracts();
}
} // namespace nenenib::tests
