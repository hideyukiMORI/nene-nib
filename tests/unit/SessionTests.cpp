// scope `--session` の単体テスト（ADR 0042 決定 2・ADR 0059 の決定 1〜6）。状態から前回のタブの
// 一覧を作る読み取りと、窓が閉じていくときの意図 EndSession。まだ読んでいない文書の状態の関数と、
// 起動で前回のタブを戻す手順。
#include "ActivateCommandChoice.hpp"
#include "Appearance.hpp"
#include "CancelCommand.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "Document.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "EndSession.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenTabList.hpp"
#include "PlaceCaret.hpp"
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
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionFailure.hpp"
#include "SessionTab.hpp"
#include "SettleRecentTab.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "ThemeCatalog.hpp"
#include "UnloadedDocument.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::ActivateCommandChoice;
using nenenib::application::CancelCommand;
using nenenib::application::CloseTab;
using nenenib::application::Document;
using nenenib::application::DocumentState;
using nenenib::application::DocumentView;
using nenenib::application::EditorFrame;
using nenenib::application::EditorPorts;
using nenenib::application::EditorState;
using nenenib::application::EndSession;
using nenenib::application::InsertText;
using nenenib::application::NewTab;
using nenenib::application::OpenCommandPalette;
using nenenib::application::OpenDocument;
using nenenib::application::OpenTabList;
using nenenib::application::PlaceCaret;
using nenenib::application::ScrollLines;
using nenenib::application::SelectEditMode;
using nenenib::application::Session;
using nenenib::application::SessionEnd;
using nenenib::application::SessionFailure;
using nenenib::application::SessionTab;
using nenenib::application::SettleRecentTab;
using nenenib::application::SwitchTab;
using nenenib::application::UnloadedDocument;
using nenenib::application::VisibleLines;
using nenenib::application::WalkRecentTab;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::FilePath;
using nenenib::core::LineNumber;
using nenenib::core::SaveState;
using nenenib::core::SelectionAnchoring;
using nenenib::core::tab_title_for;
using nenenib::core::TabStep;
using nenenib::core::TextEncoding;
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

// ---------------------------------------------------------------- 戻す（#253・ADR 0059 の決定
// 4〜6）

[[nodiscard]] FilePath work_path(std::string_view name)
{
    auto parsed = FilePath::parse("C:\\work\\" + std::string(name) + ".txt");
    expect(parsed.has_value(), "the restore contract path parses");
    return std::move(parsed).value();
}

[[nodiscard]] UnloadedDocument unloaded(std::string_view name)
{
    const FilePath path = work_path(name);
    return UnloadedDocument{path, TextPosition{LineNumber{2}, Column{3}}, LineNumber{2},
                            DocumentView{tab_title_for(path, SaveState::saved), path,
                                         TextEncoding::utf8, SaveState::saved, std::nullopt}};
}

// 無題 1 本の右に a b c のまだ読んでいない文書を並べた状態。
[[nodiscard]] EditorState restored_state()
{
    return EditorState::create(Appearance::dark, EditMode::ordinary)
        .with_restored({unloaded("a"), unloaded("b"), unloaded("c")});
}

[[nodiscard]] DocumentState bundle_of(std::string_view name)
{
    const FilePath path = work_path(name);
    const Document document{path, TextEncoding::utf8, std::size_t{0}};
    return DocumentState{buffer_of(name),
                         nenenib::core::collapsed_at(nenenib::core::Offset{0}),
                         nenenib::core::EditHistory::empty(),
                         document,
                         LineNumber{1},
                         std::nullopt,
                         std::nullopt,
                         DocumentView{tab_title_for(path, SaveState::saved), path,
                                      TextEncoding::utf8, SaveState::saved, std::nullopt}};
}

// 使った順を帯の位置の数字の並びで読む（先頭がいちばん最近）。
[[nodiscard]] std::string order_of(const EditorState &state)
{
    std::string text;
    for (const std::size_t position : state.recency().order())
    {
        text += std::to_string(position);
    }
    return text;
}

// 帯・アクティブ・脇の参照・使った順・歩きの印が同じ（何も変えていない）。
[[nodiscard]] bool same_band(const EditorState &left, const EditorState &right)
{
    return left.tab_count() == right.tab_count() && left.active_tab() == right.active_tab() &&
           left.parked() == right.parked() && order_of(left) == order_of(right) &&
           left.tab_walking() == right.tab_walking() && left.text().text() == right.text().text();
}

[[nodiscard]] std::string unloaded_name(const EditorState &state, std::size_t position)
{
    const auto document = state.unloaded_at(position);
    return document.has_value() ? std::string(document.value().path.file_name()) : std::string("-");
}

// 一覧のタブを右へ並べ、アクティブは動かさず先頭のまま（決定 4・6）。
void verify_restored_state()
{
    const EditorState state = restored_state();
    expect(state.tab_count() == 4 && state.active_tab() == 0 && state.parked().size() == 3,
           "restoring puts the listed tabs right of the untitled tab");
    expect(unloaded_name(state, 0) == "-" && unloaded_name(state, 1) == "a.txt" &&
               unloaded_name(state, 2) == "b.txt" && unloaded_name(state, 3) == "c.txt" &&
               unloaded_name(state, 9) == "-",
           "the restored tabs are unloaded in band order and the active one is not");
    expect(order_of(state) == "0321", "the restored tabs join the order behind the active tab");
    const auto edited = state.with_edit(
        buffer_of("x"), nenenib::core::collapsed_at(nenenib::core::Offset{1}), state.history());
    expect(edited.parked() == state.parked(),
           "typing into the active document shares the unloaded tabs");
}

// 置き換えと外しの各枝（決定 5）。
void verify_loaded_and_dropped()
{
    const EditorState state = restored_state();
    const EditorState loaded = state.with_loaded(2, bundle_of("b"));
    expect(loaded.tab_count() == 4 && unloaded_name(loaded, 2) == "-" &&
               unloaded_name(loaded, 1) == "a.txt" && unloaded_name(loaded, 3) == "c.txt" &&
               loaded.parked().at(0) == state.parked().at(0) &&
               loaded.parked().at(2) == state.parked().at(2) && order_of(loaded) == "0321",
           "loading replaces only that tab with a bundle");
    expect(same_band(state.with_loaded(0, bundle_of("x")), state) &&
               same_band(state.with_loaded(9, bundle_of("x")), state) &&
               same_band(loaded.with_loaded(2, bundle_of("x")), loaded),
           "loading the active, an out-of-range or a loaded tab changes nothing");
    const EditorState switched = loaded.with_switched(2);
    expect(switched.active_tab() == 2 && switched.text().text() == "b" &&
               order_of(switched) == "2031",
           "a loaded tab can be switched to");
    const EditorState left = switched.with_dropped(1);
    expect(left.tab_count() == 3 && left.active_tab() == 1 && left.text().text() == "b" &&
               unloaded_name(left, 2) == "c.txt" && order_of(left) == "102",
           "dropping an unloaded tab left of the active one shifts the active position");
    const EditorState right = switched.with_dropped(3);
    expect(right.tab_count() == 3 && right.active_tab() == 2 &&
               unloaded_name(right, 1) == "a.txt" && order_of(right) == "201",
           "dropping an unloaded tab right of the active one keeps the active position");
    expect(same_band(switched.with_dropped(2), switched) &&
               same_band(switched.with_dropped(0), switched) &&
               same_band(switched.with_dropped(9), switched),
           "dropping the active, a loaded or an out-of-range tab changes nothing");
}

// 切り替えの 3 つの関数は、まだ読んでいない文書を指されたら何も変えない（決定 5）。
void verify_switch_refuses_unloaded()
{
    const EditorState state = restored_state();
    expect(same_band(state.with_switched(1), state),
           "switching to an unloaded tab changes nothing");
    expect(same_band(state.with_walked(3), state) && !state.with_walked(3).tab_walking(),
           "walking to an unloaded tab changes nothing");
    expect(same_band(state.with_closed(0), state),
           "closing the active tab whose right neighbour is unloaded changes nothing");
    const EditorState closed = state.with_closed(2);
    expect(closed.tab_count() == 3 && unloaded_name(closed, 2) == "c.txt" &&
               closed.active_tab() == 0,
           "an unloaded tab that is not active can still be closed");
    const EditorState pair = EditorState::create(Appearance::dark, EditMode::ordinary)
                                 .with_restored({unloaded("a"), unloaded("b")})
                                 .with_loaded(2, bundle_of("b"))
                                 .with_switched(2);
    expect(pair.active_tab() == 2 && same_band(pair.with_closed(2), pair),
           "closing the rightmost active tab whose left neighbour is unloaded changes nothing");
}

// 使った順を順位から作り直す（決定 6）。本数が違えば何も変えない。
void verify_recency_ranked()
{
    const EditorState state = restored_state();
    const std::vector<std::size_t> ranks{3, 1, 0, 2};
    expect(order_of(state.with_recency_ranked(ranks)) == "2130",
           "the order follows the ranks with rank 0 first");
    const std::vector<std::size_t> ties{1, 0, 1, 0};
    expect(order_of(state.with_recency_ranked(ties)) == "1302", "equal ranks keep the band order");
    const std::vector<std::size_t> short_ranks{0, 1};
    expect(same_band(state.with_recency_ranked(short_ranks), state),
           "ranks of another length change nothing");
}

[[nodiscard]] SessionTab listed_tab(std::string_view name, TextPosition caret, std::size_t first,
                                    std::size_t rank)
{
    return SessionTab{work_path(name), caret, LineNumber{first}, rank};
}

[[nodiscard]] SessionReading listed(std::vector<SessionTab> tabs, std::size_t active)
{
    return SessionReading{std::optional<Session>{Session{std::move(tabs), active}}};
}

// a b c の一覧で b を見ていた。a は 2 行 2 桁で先頭 3 行、b は 4 行 1 桁で先頭 2 行。
[[nodiscard]] SessionReading three_listed(std::size_t active)
{
    return listed({listed_tab("a", TextPosition{LineNumber{2}, Column{2}}, 3, 1),
                   listed_tab("b", TextPosition{LineNumber{4}, Column{1}}, 2, 0),
                   listed_tab("c", TextPosition{LineNumber{1}, Column{1}}, 1, 2)},
                  active);
}

[[nodiscard]] std::string notice(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

[[nodiscard]] std::string titles(const EditorFrame &frame)
{
    std::string text;
    for (const auto &tab : frame.tabs)
    {
        text += std::string(tab.title.text()) + " ";
    }
    return text + ">" + std::to_string(frame.active_tab);
}

// 起動の controller を組んで最初の表示値を返す。ポートは controller より先に宣言する。
[[nodiscard]] EditorFrame started_with(ScriptedSession &session, ScriptedThemes &themes,
                                       const std::vector<OpenDocument> &initial)
{
    ScriptedAppearance appearance{Reading{Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedFiles files;
    hold_three(files);
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    const EditorController controller(
        EditorPorts{appearance, clipboard, files, code_pages, settings, themes, session}, initial);
    return controller.frame();
}

// 起動の 4 つの枝（決定 6・D24）。
void verify_restore_branches()
{
    Editing none;
    expect(titles(none.controller().frame()) == "無題 >0" &&
               notice(none.controller().frame()) == "none" && none.session().reads() == 1 &&
               none.files().reads() == 0,
           "no list starts with one untitled tab and no notice");
    Editing empty(listed({}, 0), hold_three);
    expect(titles(empty.controller().frame()) == "無題 >0" &&
               notice(empty.controller().frame()) == "none" && empty.files().reads() == 0,
           "an empty list starts with one untitled tab and no notice");
    Editing broken(SessionReading{std::unexpected(SessionFailure::malformed)}, hold_three);
    expect(titles(broken.controller().frame()) == "無題 >0" &&
               notice(broken.controller().frame()) == "前回のタブを読めませんでした" &&
               !broken.controller().frame().document.last_failure.has_value(),
           "an unreadable list starts with one untitled tab and one notice line");
    ScriptedSession given(three_listed(1));
    ScriptedThemes themes;
    const EditorFrame opened = started_with(given, themes, {open_at("C:\\work\\c.txt")});
    expect(titles(opened) == "c.txt >0" && given.reads() == 0,
           "a file argument opens only that file and never reads the list");
    ScriptedSession failing(SessionReading{std::unexpected(SessionFailure::unsupported_version)});
    ScriptedThemes noticing(nenenib::core::ThemeCatalog::builtins(), fixed_text("theme notice"));
    expect(notice(started_with(failing, noticing, {})) == "前回のタブを読めませんでした",
           "the restore notice wins over the theme notice");
    ScriptedSession quiet;
    expect(notice(started_with(quiet, noticing, {})) == "theme notice",
           "the theme notice stays when the restore has nothing to say");
}

// 戻った後の帯・アクティブ・位置・使った順。読むのはアクティブの 1 本だけ（決定 6・D25・D27）。
void verify_restore_band()
{
    Editing editing(three_listed(1), hold_three);
    auto &controller = editing.controller();
    const EditorFrame frame = controller.frame();
    expect(titles(frame) == "a.txt b.txt c.txt >1" && frame.document.title.text() == "b.txt" &&
               caret_at(frame, 4, 1) && frame.first_visible.value == 2 && notice(frame) == "none" &&
               !frame.document.last_failure.has_value(),
           "the listed tabs come back and the watched tab shows its caret and first line");
    expect(editing.files().reads() == 1 && editing.files().read_path() == "C:\\work\\b.txt" &&
               editing.session().reads() == 1,
           "starting reads only the active file");
    bool saved = true;
    for (const auto &tab : frame.tabs)
    {
        saved = saved && tab.save_state == SaveState::saved;
    }
    expect(saved, "no restored tab is unsaved");
    expect(written(closed_window(editing)) == "a.txt@2:2/3#1 b.txt@4:1/2#0 c.txt@1:1/1#2 >1",
           "the list writes unloaded tabs at their remembered positions and ranks");
    const EditorFrame walked = controller.apply(WalkRecentTab{TabStep::next});
    expect(walked.active_tab == 0 && controller.tab_walking() && editing.files().reads() == 2 &&
               caret_at(walked, 2, 2),
           "a walk step towards an unloaded tab reads it and walks there");
    Editing out_of_range(listed({listed_tab("a", TextPosition{LineNumber{1}, Column{1}}, 1, 1),
                                 listed_tab("b", TextPosition{LineNumber{1}, Column{1}}, 1, 0)},
                                7),
                         hold_three);
    expect(titles(out_of_range.controller().frame()) == "a.txt b.txt >1",
           "an active position past the list falls back to the last tab");
}

[[nodiscard]] EditorFrame restored_one(std::string_view bytes, TextPosition caret,
                                       std::size_t first)
{
    const std::string content(bytes);
    Editing editing(listed({listed_tab("a", caret, first, 0)}, 0), [&content](ScriptedFiles &files)
                    { files.hold_at("C:\\work\\a.txt", Bytes{content}); });
    return editing.controller().frame();
}

// 位置は読んだ本文の範囲の中へ寄せる（決定 5・D25）。
void verify_restore_clamps()
{
    const EditorFrame shrank = restored_one("a\nl2\nl3", TextPosition{LineNumber{9}, Column{4}}, 9);
    expect(caret_at(shrank, 3, 3) && shrank.first_visible.value == 3,
           "a line past the end moves to the last line and its end");
    const EditorFrame wide = restored_one("abc\nd", TextPosition{LineNumber{1}, Column{10}}, 1);
    expect(caret_at(wide, 1, 4), "a column past the line end moves to the line end");
    const std::string full = "あいう";
    Editing editing(listed({listed_tab("a", TextPosition{LineNumber{1}, Column{2}}, 1, 0)}, 0),
                    [&full](ScriptedFiles &files)
                    { files.hold_at("C:\\work\\a.txt", Bytes{full}); });
    const EditorFrame typed = editing.controller().apply(InsertText{"x"});
    expect(vim_body(typed) == "あxいう", "a full-width line keeps the caret between characters");
    const EditorFrame blank = restored_one("", TextPosition{LineNumber{5}, Column{5}}, 5);
    expect(caret_at(blank, 1, 1) && blank.first_visible.value == 1,
           "an empty file puts the caret and the first line at the top");
}

// 読めないタブは外れて 1 行知らせ、隣を試す（決定 5・6・D26）。
void verify_restore_unreadable()
{
    const auto failing = [](std::string_view name, FileFailure failure)
    {
        return [name, failure](ScriptedFiles &files)
        {
            hold_three(files);
            files.hold_at("C:\\work\\" + std::string(name) + ".txt",
                          Bytes{std::unexpected(failure)});
        };
    };
    Editing missing(three_listed(0), failing("a", FileFailure::not_found));
    const EditorFrame gone = missing.controller().frame();
    expect(titles(gone) == "b.txt c.txt >0" && notice(gone) == "開けませんでした: a.txt" &&
               !gone.document.last_failure.has_value() && missing.files().reads() == 2,
           "a missing active file is dropped, noticed and its right neighbour is read");
    Editing large(three_listed(2), failing("c", FileFailure::too_large));
    expect(titles(large.controller().frame()) == "a.txt b.txt >1" &&
               notice(large.controller().frame()) == "開けませんでした: c.txt",
           "a too large rightmost file falls back to its left neighbour");
    Editing garbled(three_listed(1), failing("b", FileFailure::undecodable));
    expect(titles(garbled.controller().frame()) == "a.txt c.txt >1" &&
               notice(garbled.controller().frame()) == "開けませんでした: b.txt",
           "an undecodable file is dropped the same way");
    Editing twice(three_listed(2),
                  [](ScriptedFiles &files)
                  {
                      files.hold_at("C:\\work\\a.txt", Bytes{"a"});
                      files.hold_at("C:\\work\\b.txt",
                                    Bytes{std::unexpected(FileFailure::not_found)});
                  });
    expect(titles(twice.controller().frame()) == "a.txt >0" &&
               notice(twice.controller().frame()) == "開けませんでした: b.txt（ほか 1 件）" &&
               twice.files().reads() == 3,
           "two failures name the last file and count the other");
    Editing nothing(three_listed(1), [](ScriptedFiles &) {});
    expect(titles(nothing.controller().frame()) == "無題 >0" &&
               notice(nothing.controller().frame()) == "開けませんでした: a.txt（ほか 2 件）" &&
               nothing.files().reads() == 3,
           "when no tab can be read the untitled tab stays");
}

// 名前の文字のファイルを読めなくする（ほかは hold_three のまま）。
void hold_without(ScriptedFiles &files, std::string_view names)
{
    hold_three(files);
    for (const char name : names)
    {
        files.hold_at("C:\\work\\" + std::string(1, name) + ".txt",
                      Bytes{std::unexpected(FileFailure::not_found)});
    }
}

// 切り替えの入口はまだ読んでいない文書をそこで読み、覚えていた位置に着く（決定 5・D27）。
void verify_switch_reads()
{
    Editing clicked(three_listed(1), hold_three);
    auto &controller = clicked.controller();
    const EditorFrame first = controller.apply(SwitchTab{0});
    expect(titles(first) == "a.txt b.txt c.txt >0" && first.document.title.text() == "a.txt" &&
               caret_at(first, 2, 2) && first.first_visible.value == 3 &&
               clicked.files().reads() == 2 && clicked.files().read_path() == "C:\\work\\a.txt",
           "a click on an unloaded tab reads it and shows its caret and first line");
    const EditorFrame back = controller.apply(SwitchTab{1});
    expect(back.document.title.text() == "b.txt" && caret_at(back, 4, 1) &&
               clicked.files().reads() == 2,
           "switching back to a loaded tab reads nothing");
    const EditorFrame ex = run_ex(controller, "tabnext 3");
    expect(ex.active_tab == 2 && ex.document.title.text() == "c.txt" &&
               clicked.files().reads() == 3,
           ":tabnext reads the unloaded tab");
    applied(controller, SwitchTab{0});
    expect(clicked.files().reads() == 3, "a tab read once is not read again");

    Editing vim(three_listed(1), hold_three);
    applied(vim.controller(), SelectEditMode{EditMode::vim});
    vim_replay(vim.controller(), "gt");
    const EditorFrame gt = vim.controller().frame();
    expect(gt.active_tab == 2 && gt.document.title.text() == "c.txt" && vim.files().reads() == 2,
           "gt reads the unloaded tab");

    Editing listed_tabs(three_listed(1), hold_three);
    applied(listed_tabs.controller(), OpenTabList{});
    const EditorFrame chosen = listed_tabs.controller().apply(ActivateCommandChoice{0});
    expect(chosen.active_tab == 0 && chosen.document.title.text() == "a.txt" &&
               !chosen.command_palette.has_value() && listed_tabs.files().reads() == 2,
           "a row of the tab list reads the unloaded tab");

    Editing opened(three_listed(1), hold_three);
    const EditorFrame same = opened.controller().apply(open_at("C:\\work\\c.txt"));
    expect(titles(same) == "a.txt b.txt c.txt >2" && opened.files().reads() == 2,
           "opening the file of an unloaded tab switches there and reads it once");
}

// 歩きの 1 歩も同じ 1 本で読む。読めなければ外れて、歩きは今のタブのまま続く（決定 5）。
void verify_walk_reaches()
{
    Editing walking(three_listed(1), hold_three);
    auto &controller = walking.controller();
    applied(controller, WalkRecentTab{TabStep::next});
    const EditorFrame second = controller.apply(WalkRecentTab{TabStep::next});
    expect(second.active_tab == 2 && controller.tab_walking() && walking.files().reads() == 3,
           "each walk step reads the tab it reaches");
    const EditorFrame settled = controller.apply(SettleRecentTab{});
    expect(settled.active_tab == 2 && !controller.tab_walking(), "the walk settles as before");

    Editing missing(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "a"); });
    auto &walker = missing.controller();
    const EditorFrame dropped = walker.apply(WalkRecentTab{TabStep::next});
    expect(titles(dropped) == "b.txt c.txt >0" && !walker.tab_walking() &&
               notice(dropped) == "開けませんでした: a.txt",
           "an unreadable walk target is dropped and the walk stays on the current tab");
    const EditorFrame next = walker.apply(WalkRecentTab{TabStep::next});
    expect(next.active_tab == 1 && next.document.title.text() == "c.txt" && walker.tab_walking() &&
               notice(next) == "none",
           "the next step goes to the neighbour in the order after the drop");

    Editing mid(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "c"); });
    applied(mid.controller(), WalkRecentTab{TabStep::next});
    const EditorFrame stuck = mid.controller().apply(WalkRecentTab{TabStep::next});
    expect(titles(stuck) == "a.txt b.txt >0" && mid.controller().tab_walking(),
           "a drop in the middle of a walk keeps walking");
}

// 読めない行き先は外れて 1 行知らせ、アクティブと入力行はそのまま。知らせは意図ごとに数える。
void verify_switch_unreadable()
{
    Editing editing(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "ac"); });
    auto &controller = editing.controller();
    applied(controller, OpenCommandPalette{});
    const EditorFrame first = controller.apply(SwitchTab{0});
    expect(titles(first) == "b.txt c.txt >0" && first.document.title.text() == "b.txt" &&
               caret_at(first, 4, 1) && notice(first) == "開けませんでした: a.txt" &&
               first.command_palette.has_value() && !first.document.last_failure.has_value(),
           "an unreadable tab is dropped, noticed and the open palette stays");
    applied(controller, CancelCommand{});
    const EditorFrame second = run_ex(controller, "tabnext 2");
    expect(titles(second) == "b.txt >0" && notice(second) == "開けませんでした: c.txt",
           "a failure in a later intent is counted afresh");

    Editing vim(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "c"); });
    applied(vim.controller(), SelectEditMode{EditMode::vim});
    vim_replay(vim.controller(), "gt");
    const EditorFrame gt = vim.controller().frame();
    expect(titles(gt) == "a.txt b.txt >1" && notice(gt) == "開けませんでした: c.txt",
           "gt towards an unreadable tab stays and notices");

    Editing opened(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "a"); });
    const EditorFrame same = opened.controller().apply(open_at("C:\\work\\a.txt"));
    expect(titles(same) == "b.txt c.txt >0" && notice(same) == "開けませんでした: a.txt" &&
               !same.document.last_failure.has_value() && opened.files().reads() == 2,
           "opening the file of an unreadable tab only notices and does not read it twice");
}

// 閉じた後の隣は読んでから移る。読めなければ次の隣、1 本も残らなければ無題（決定 5）。
void verify_close_reaches()
{
    Editing right(three_listed(1), hold_three);
    const EditorFrame closed = right.controller().apply(CloseTab{1});
    expect(titles(closed) == "a.txt c.txt >1" && closed.document.title.text() == "c.txt" &&
               right.files().reads() == 2 && !closed.closing,
           "closing the active tab reads its right neighbour");
    Editing left(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "c"); });
    const EditorFrame fallback = left.controller().apply(CloseTab{1});
    expect(titles(fallback) == "a.txt >0" && caret_at(fallback, 2, 2) &&
               notice(fallback) == "開けませんでした: c.txt" && !fallback.closing &&
               left.files().reads() == 3,
           "an unreadable neighbour is dropped and the next neighbour is read");
    Editing none(three_listed(1), [](ScriptedFiles &files) { hold_without(files, "ac"); });
    const EditorFrame blank = none.controller().apply(CloseTab{1});
    expect(titles(blank) == "無題 >0" && !blank.closing &&
               notice(blank) == "開けませんでした: a.txt（ほか 1 件）",
           "when no neighbour can be read one untitled tab stays and the window stays open");
    Editing parked(three_listed(1), hold_three);
    const EditorFrame aside = parked.controller().apply(CloseTab{0});
    expect(titles(aside) == "b.txt c.txt >0" && parked.files().reads() == 1,
           "closing an unloaded tab that is not active reads nothing");
}

// 窓を閉じるときの一覧。まだ読んでいない文書は覚えていた位置、読んだタブは今の位置（決定 1・5）。
void verify_list_after_reaching()
{
    Editing editing(three_listed(1), hold_three);
    auto &controller = editing.controller();
    applied(controller, VisibleLines{3});
    applied(controller, SwitchTab{0});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{5}, Column{1}}, SelectionAnchoring::collapse});
    expect(written(closed_window(editing)) == "a.txt@5:1/3#0 b.txt@4:1/2#1 c.txt@1:1/1#2 >0",
           "a read tab is listed at its current position and an unloaded one as remembered");
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
    verify_restored_state();
    verify_loaded_and_dropped();
    verify_switch_refuses_unloaded();
    verify_recency_ranked();
    verify_restore_branches();
    verify_restore_band();
    verify_restore_clamps();
    verify_restore_unreadable();
    verify_switch_reads();
    verify_walk_reaches();
    verify_switch_unreadable();
    verify_close_reaches();
    verify_list_after_reaching();
}

void verify_session_scope()
{
    verify_session_contracts();
}
} // namespace nenenib::tests
