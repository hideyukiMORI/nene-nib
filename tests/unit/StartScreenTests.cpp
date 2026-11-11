// 起動画面の application の契約（ADR 0172・追補 2026-11-11・#1018 の段 1）。出る条件・節と 10 件・
// 選びの鍵（通常モードの ↑ ↓ と Vim の NORMAL の j k）・Enter とクリックの実行（open_listed
// と操作の 頼み）・消える条件・guide=hidden・最初の frame の後の 1 回の読み・静かな道。
#include "CancelCommand.hpp"
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "ChooseStartEntry.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FirstFramePresented.hpp"
#include "GuideVisibility.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "PreviewPlacement.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SetPreviewPlacement.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "StartEntryKind.hpp"
#include "StartLists.hpp"
#include "StartScreen.hpp"
#include "StartScreenView.hpp"
#include "StartStep.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
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

[[nodiscard]] core::FilePath path_named(std::string_view name)
{
    return core::FilePath::parse("C:\\work\\" + std::string(name)).value();
}

[[nodiscard]] std::vector<core::FilePath> paths_named(std::string_view stem, std::size_t count)
{
    std::vector<core::FilePath> paths;
    for (std::size_t index = 0; index < count; ++index)
    {
        paths.push_back(path_named(std::string(stem) + std::to_string(index) + ".txt"));
    }
    return paths;
}

[[nodiscard]] std::optional<app::StartScreenView> shown(app::EditorController &controller)
{
    return controller.frame().start_screen;
}

// 出ていなければ空の表示値（出ているかは shown で別に見る）。
[[nodiscard]] app::StartScreenView view_of(app::EditorController &controller)
{
    return shown(controller).value_or(app::StartScreenView{});
}

[[nodiscard]] std::optional<std::size_t> selected_of(app::EditorController &controller)
{
    const auto view = shown(controller);
    return view.has_value() ? view.value().selected : std::nullopt;
}

void press_down(app::EditorController &controller)
{
    applied(controller,
            app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse});
}

void press_up(app::EditorController &controller)
{
    applied(controller,
            app::MoveCaret{core::CaretMotion::previous_line, core::SelectionAnchoring::collapse});
}

void vim_press(app::EditorController &controller, std::string_view keys)
{
    for (const auto &key : vim_keys_of(keys))
    {
        applied(controller, app::VimKeyPress{key});
    }
}

// 最初の frame の合図を送り、仕込んだ読みを合図の道で届ける。
void present_with(Editing &editing, app::StartLists lists)
{
    applied(editing.controller(), app::FirstFramePresented{});
    editing.start_lists().serve(std::move(lists));
    applied(editing.controller(), app::WorkCompleted{});
}

// 決定 1・2・3: 何も書いていない無題の起動は、最初の frame から「はじめる」の 3 行と鍵の節を出す。
// 履歴とブックマークはまだ読まず、最初の frame の合図で 1 回だけ頼む。
void verify_first_frame_and_one_read()
{
    Editing editing;
    auto &controller = editing.controller();
    const auto first = shown(controller);
    expect(first.has_value() && first.value().entries.size() == 3 && !first.value().lists_read &&
               !first.value().selected.has_value() &&
               first.value().entries.at(0).kind == core::StartEntryKind::new_file &&
               first.value().entries.at(1).kind == core::StartEntryKind::open_file &&
               first.value().entries.at(2).kind == core::StartEntryKind::open_folder,
           "the untouched untitled start shows the three start rows before anything is read");
    const auto keys = view_of(controller).keys;
    expect(!keys.at(0).key.empty() && !keys.at(1).key.empty() && !keys.at(2).key.empty(),
           "the key section shows the chords of Ctrl+P, F1 and the explorer");
    expect(editing.start_lists().touches() == 0 && editing.history().reads() == 0 &&
               editing.bookmarks().reads() == 0,
           "the start itself never asks for the history nor the bookmarks");
    // 最初の frame の前の鍵は本文へ行く（起動画面の鍵は合図の後だけ）。
    press_down(controller);
    expect(!selected_of(controller).has_value(), "a key before the first frame never selects");
    applied(controller, app::FirstFramePresented{});
    applied(controller, app::FirstFramePresented{});
    expect(editing.start_lists().requests() == 1 && editing.start_lists().collects() == 0 &&
               editing.history().reads() == 0,
           "the first frame asks the start lists once and the controller never reads by itself");
    applied(controller, app::WorkCompleted{});
    expect(editing.start_lists().collects() == 1 && shown(controller).has_value() &&
               !view_of(controller).lists_read,
           "a work signal before the lists arrive keeps the three rows");
    auto lists = app::StartLists{paths_named("recent", 12), paths_named("mark", 3)};
    editing.start_lists().serve(std::move(lists));
    applied(controller, app::WorkCompleted{});
    const auto read = view_of(controller);
    expect(read.lists_read && read.entries.size() == 10 + 3 + 3 &&
               read.entries.at(0).kind == core::StartEntryKind::recent &&
               read.entries.at(0).path == path_named("recent0.txt") &&
               read.entries.at(9).path == path_named("recent9.txt") &&
               read.entries.at(10).kind == core::StartEntryKind::bookmark &&
               read.entries.at(12).path == path_named("mark2.txt") &&
               read.entries.at(13).kind == core::StartEntryKind::new_file,
           "recent files are cut at ten in history order, then the bookmarks, then the start rows");
    applied(controller, app::WorkCompleted{});
    expect(editing.start_lists().collects() == 2 && editing.start_lists().requests() == 1,
           "after the lists arrived the work signal no longer collects them");
}

// 追補の 5: 最初は何も選ばない。↓ は最初、↑ は最後から選び、端で回る。Esc は外し、選んでいない
// Esc と Enter は本文へ行く（通常モードの Enter は改行が入って消える）。
void verify_ordinary_selection()
{
    Editing editing;
    auto &controller = editing.controller();
    present_with(editing, app::StartLists{paths_named("recent", 2), {}});
    press_down(controller);
    expect(selected_of(controller) == std::optional<std::size_t>{0}, "the first down selects 0");
    press_up(controller);
    expect(selected_of(controller) == std::optional<std::size_t>{4},
           "up from the first row wraps to the last row");
    applied(controller, app::CancelSelection{});
    expect(shown(controller).has_value() && !selected_of(controller).has_value(),
           "Esc clears the selection and the start screen stays");
    press_up(controller);
    expect(selected_of(controller) == std::optional<std::size_t>{4},
           "the first up selects the last row");
    applied(controller, app::CancelSelection{});
    applied(controller, app::CancelSelection{});
    expect(shown(controller).has_value(), "Esc without a selection changes nothing");
    applied(controller, app::NewLine{});
    expect(!shown(controller).has_value() && controller.frame().total_lines == 2,
           "Enter without a selection types a new line and the start screen goes away");
}

// 決定 4: 選んだ行の Enter とクリックは open_listed の 1 本か操作の頼み。
void verify_running_rows()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\work\\recent0.txt", Bytes{std::string("first")});
    present_with(editing, app::StartLists{paths_named("recent", 2), paths_named("mark", 1)});
    press_down(controller);
    applied(controller, app::NewLine{});
    const auto opened = controller.frame();
    expect(!opened.start_screen.has_value() && opened.document.path == path_named("recent0.txt") &&
               opened.lines.size() == 1 && opened.lines.at(0).text == "first",
           "Enter on a recent row opens that file and the start screen goes away");
}

void verify_failed_rows()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.history().serve(app::FileHistory{paths_named("recent", 2)});
    present_with(editing, app::StartLists{paths_named("recent", 2), paths_named("mark", 1)});
    const auto writes = editing.history().writes();
    applied(controller, app::ChooseStartEntry{1});
    const auto missing = shown(controller);
    expect(missing.has_value() && missing.value().entries.size() == 1 + 1 + 3 &&
               missing.value().entries.at(0).path == path_named("recent0.txt") &&
               controller.frame().command_message.has_value() &&
               editing.history().writes() == writes + 1,
           "a click on a missing recent file notices one line, forgets it in the history and "
           "drops it from the rows");
    applied(controller, app::ChooseStartEntry{1});
    const auto mark = shown(controller);
    expect(mark.has_value() && mark.value().entries.size() == 5 &&
               mark.value().entries.at(1).kind == core::StartEntryKind::bookmark &&
               controller.frame().command_message.has_value() &&
               editing.history().writes() == writes + 1,
           "a missing bookmark notices one line and stays (ADR 0063)");
    applied(controller, app::ChooseStartEntry{99});
    expect(shown(controller).has_value(), "a click outside the rows does nothing");
}

void verify_start_rows()
{
    Editing editing;
    auto &controller = editing.controller();
    present_with(editing, app::StartLists{});
    applied(controller, app::ChooseStartEntry{1});
    expect(controller.frame().operation_request ==
               std::optional<core::EditorOperation>{core::EditorOperation::open_file},
           "the open file row asks the Ctrl+O operation");
    press_up(controller);
    applied(controller, app::NewLine{});
    expect(controller.frame().operation_request ==
               std::optional<core::EditorOperation>{core::EditorOperation::choose_explorer_root},
           "Enter on the open folder row asks to choose the explorer root");
    press_down(controller);
    applied(controller, app::NewLine{});
    expect(shown(controller).has_value() && !selected_of(controller).has_value() &&
               !controller.frame().operation_request.has_value() &&
               controller.frame().total_lines == 1,
           "the new file row only clears the selection (typing starts the file)");
}

// 決定 1: 打つと消え、guide=hidden では出ない。入力行と面の間も出さない（追補の 5）。
void verify_conditions()
{
    {
        Editing editing;
        auto &controller = editing.controller();
        present_with(editing, app::StartLists{});
        press_down(controller);
        applied(controller, app::InsertText{"a"});
        expect(!shown(controller).has_value(), "typing makes the document touched");
    }
    {
        auto settings = core::default_editor_settings();
        settings.guide = core::GuideVisibility::hidden;
        Editing hidden(settings);
        applied(hidden.controller(), app::FirstFramePresented{});
        expect(!shown(hidden.controller()).has_value() && hidden.start_lists().requests() == 0,
               "guide=hidden hides the start screen and never asks for the lists");
    }
    Editing editing;
    auto &controller = editing.controller();
    present_with(editing, app::StartLists{});
    press_down(controller);
    applied(controller, app::OpenCommandPalette{});
    expect(!shown(controller).has_value(), "the Ctrl+P panel hides the start screen");
    applied(controller, app::CancelCommand{});
    expect(shown(controller).has_value() && !selected_of(controller).has_value(),
           "closing the panel shows it again with nothing selected");
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    expect(shown(controller).has_value(), "the active pane of a split still shows it");
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    const auto full = controller.frame();
    expect(full.full_preview.has_value() != full.start_screen.has_value(),
           "the full preview never shows the start screen");
}

// 追補の 1: 状態は持たない。新しい無題のタブにも出て、別の文書がアクティブなら出ない。引数で
// 起動したなら読みを頼まず、後で無題のタブを足しても読まない。
void verify_tabs_and_quiet_start()
{
    Editing editing(SessionReading{std::nullopt},
                    std::vector<app::OpenDocument>{app::OpenDocument{path_named("given.txt")}},
                    [](ScriptedFiles &files, ScriptedDocumentReads &)
                    { files.hold_at("C:\\work\\given.txt", Bytes{std::string("given")}); });
    auto &controller = editing.controller();
    expect(!shown(controller).has_value(), "a start with a file shows no start screen");
    applied(controller, app::FirstFramePresented{});
    applied(controller, app::NewTab{});
    const auto added = shown(controller);
    expect(added.has_value() && !added.value().lists_read && added.value().entries.size() == 3 &&
               editing.start_lists().touches() == 0 && editing.history().reads() == 0,
           "a new untitled tab shows the start rows without reading the lists");
    press_down(controller);
    expect(selected_of(controller) == std::optional<std::size_t>{0},
           "the new tab's start screen takes the keys");
    applied(controller, app::SwitchTab{0});
    expect(!shown(controller).has_value(), "another active document shows no start screen");
    applied(controller, app::SwitchTab{1});
    expect(shown(controller).has_value() && !selected_of(controller).has_value(),
           "back on the untitled tab nothing is selected");
}

// 決定 4・追補の 5: Vim は NORMAL で次の鍵を待っていないときだけ出し、j k と矢印で選ぶ。INSERT・
// VISUAL・オペレータ待ち・入力行では出さず、NORMAL へ戻れば何も選んでいない形でまた出る。
void verify_vim()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\work\\recent0.txt", Bytes{std::string("vim")});
    present_with(editing, app::StartLists{paths_named("recent", 1), {}});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    expect(shown(controller).has_value(), "Vim NORMAL shows the start screen");
    vim_press(controller, "jj");
    expect(selected_of(controller) == std::optional<std::size_t>{1}, "j selects downwards");
    vim_press(controller, "k");
    expect(selected_of(controller) == std::optional<std::size_t>{0}, "k selects upwards");
    vim_press(controller, "<Esc>");
    expect(!selected_of(controller).has_value() && shown(controller).has_value(),
           "Esc clears the selection");
    vim_press(controller, "ji");
    expect(!shown(controller).has_value(), "INSERT hides the start screen");
    vim_press(controller, "<Esc>");
    expect(shown(controller).has_value() && !selected_of(controller).has_value(),
           "back in NORMAL it shows again with nothing selected");
    vim_press(controller, "d");
    expect(!shown(controller).has_value(), "an operator waiting hides it");
    vim_press(controller, "<Esc>v");
    expect(!shown(controller).has_value(), "VISUAL hides it");
    vim_press(controller, "<Esc>:");
    expect(!shown(controller).has_value(), "the Ex input line hides it");
    applied(controller, app::CancelCommand{});
    expect(shown(controller).has_value(), "closing the Ex input line shows it again");
    vim_press(controller, "<Down><CR>");
    expect(controller.frame().document.path == path_named("recent0.txt") &&
               !shown(controller).has_value(),
           "the arrow and Enter run the selected row in NORMAL");
}

void verify_selection_steps()
{
    using core::StartStep;
    expect(!core::start_selection_stepped(std::nullopt, 0, StartStep::next).has_value() &&
               core::start_selection_stepped(std::nullopt, 3, StartStep::next) ==
                   std::optional<std::size_t>{0} &&
               core::start_selection_stepped(std::nullopt, 3, StartStep::previous) ==
                   std::optional<std::size_t>{2} &&
               core::start_selection_stepped(2, 3, StartStep::next) ==
                   std::optional<std::size_t>{0} &&
               core::start_selection_stepped(0, 3, StartStep::previous) ==
                   std::optional<std::size_t>{2},
           "the selection starts at either end and wraps at the ends");
}
} // namespace

void verify_start_screen_scope()
{
    verify_selection_steps();
    verify_first_frame_and_one_read();
    verify_ordinary_selection();
    verify_running_rows();
    verify_failed_rows();
    verify_start_rows();
    verify_conditions();
    verify_tabs_and_quiet_start();
    verify_vim();
}
} // namespace nenenib::tests
