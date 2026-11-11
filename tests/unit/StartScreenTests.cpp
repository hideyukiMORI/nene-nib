// 起動画面の application の契約（ADR 0172・追補 2026-11-11・#1018 の段 1）。出る条件・節と 10 件・
// 選びの鍵（通常モードの ↑ ↓ と Vim の NORMAL の j k）・Enter とクリックの実行（open_listed
// と操作の 頼み）・消える条件・guide=hidden・最初の frame の後の 1 回の読み・静かな道。
// 段 2（#1455）: core の配置 start_screen_layout と当たり start_screen_row_at（描画と窓のクリックが
// 同じ 1 本を読む・中央寄せの 1 列・幅 480 DIP・狭い窓・DPI 100 / 150 / 200%）。
#include "BodyLayout.hpp"
#include "CancelCommand.hpp"
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "ChooseStartEntry.hpp"
#include "DevicePixels.hpp"
#include "DocumentReadFinished.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorOperation.hpp"
#include "EditorSettings.hpp"
#include "FileFailure.hpp"
#include "FileHistory.hpp"
#include "FilePath.hpp"
#include "FirstFramePresented.hpp"
#include "FontSize.hpp"
#include "GuideVisibility.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
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
#include "StartPress.hpp"
#include "StartScreen.hpp"
#include "StartScreenLayout.hpp"
#include "StartScreenShape.hpp"
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
#include <cstdint>
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
               opened.lines.size() == 1 && opened.lines.at(0).text == "first" &&
               !opened.document.transient,
           "Enter on a recent row opens that file as a pinned tab and the start screen goes away");
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

[[nodiscard]] std::uint64_t last_ticket(Editing &editing)
{
    const auto &requests = editing.reads().requests();
    return requests.empty() ? 0 : requests.back().ticket;
}

// 追補 11: 裏で読む最近のファイル。読んでいる間は起動画面を出し続け、読めたら文書が替わって消える。
// 開けたファイルは行に残る。裏の読みが「無い」で終われば履歴から外し、行からも外す（forget の 1
// 本）。
void verify_background_rows()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\work\\recent0.txt", Bytes{std::string("zero")});
    editing.files().hold_at("C:\\work\\recent1.txt", Bytes{std::string("one")});
    editing.reads().defer_above(0);
    editing.history().serve(app::FileHistory{paths_named("recent", 2)});
    present_with(editing, app::StartLists{paths_named("recent", 2), {}});
    press_down(controller);
    applied(controller, app::NewLine{});
    const auto reading = shown(controller);
    expect(reading.has_value() && reading.value().entries.size() == 2 + 3 &&
               !reading.value().selected.has_value(),
           "while the recent file is read in the background the start screen stays");
    editing.reads().finish(last_ticket(editing));
    applied(controller, app::WorkCompleted{});
    expect(!shown(controller).has_value() &&
               controller.frame().document.path == path_named("recent0.txt"),
           "the background read opens the file and the start screen goes away");
    applied(controller, app::NewTab{});
    expect(view_of(controller).entries.size() == 2 + 3 &&
               view_of(controller).entries.at(0).path == path_named("recent0.txt"),
           "a file opened in the background stays in the recent rows");
    const auto writes = editing.history().writes();
    applied(controller, app::ChooseStartEntry{1});
    editing.reads().serve(app::DocumentReadFinished{last_ticket(editing),
                                                    std::unexpected(app::FileFailure::not_found)});
    applied(controller, app::WorkCompleted{});
    const auto failed = view_of(controller);
    expect(shown(controller).has_value() && failed.entries.size() == 1 + 3 &&
               failed.entries.at(0).path == path_named("recent0.txt") &&
               controller.frame().command_message.has_value() &&
               editing.history().writes() == writes + 1,
           "a background read that finds no file forgets it in the history and in the rows");
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
[[nodiscard]] core::BodyLayout wide_body(std::uint32_t dpi)
{
    const core::LayoutRect band{0, core::to_pixels(40, dpi), core::to_pixels(1300, dpi),
                                core::to_pixels(800, dpi)};
    return core::body_layout_in(band, dpi, core::default_font_size());
}

[[nodiscard]] std::pair<std::int32_t, std::int32_t> middle_of(const core::LayoutRect &area)
{
    return {(area.left + area.right) / 2, (area.top + area.bottom) / 2};
}

// 列の中央・幅・題（段 2 の配置の採用値）。
void verify_column(const core::StartScreenLayout &layout, const core::BodyLayout &body,
                   std::uint32_t dpi)
{
    expect(core::width_of(layout.column) == core::to_pixels(core::start_column_dips, dpi),
           "the column is 480 DIP wide");
    const auto left_room = layout.column.left - body.content.left;
    const auto right_room = body.content.right - layout.column.right;
    expect(left_room >= 0 && right_room >= 0 && right_room - left_room >= 0 &&
               right_room - left_room <= 1,
           "the column sits in the middle of the text area");
    expect(layout.column.top == body.content.top + core::to_pixels(core::start_top_dips, dpi) &&
               layout.title.top == layout.column.top &&
               core::height_of(layout.title) == (body.line_height * 3) / 2,
           "the title opens the column at one and a half rows");
}

// 行は列の全幅で本文の行の高さ。節の見出しは行の上、「はじめる」の行だけ右端に補足の欄を持つ。
void verify_rows(const core::StartScreenLayout &layout, const core::BodyLayout &body)
{
    expect(layout.rows.size() == 2 + 1 + 3, "every selectable entry has a row");
    bool rows_fit = layout.rows.size() == 6;
    for (const auto &row : layout.rows)
    {
        rows_fit = rows_fit && row.line.left == layout.column.left &&
                   row.line.right == layout.column.right &&
                   core::height_of(row.line) == body.line_height &&
                   row.label.left >= row.mark.right && row.label.right <= row.line.right;
    }
    expect(rows_fit, "each row spans the column at the body row height");
    if (!rows_fit)
    {
        return;
    }
    const auto &rows = layout.rows;
    expect(rows.at(1).line.top == rows.at(0).line.bottom && layout.recent_heading.has_value() &&
               layout.bookmark_heading.has_value() &&
               layout.recent_heading.value().bottom <= rows.at(0).line.top &&
               layout.bookmark_heading.value().top >= rows.at(1).line.bottom &&
               layout.bookmark_heading.value().bottom <= rows.at(2).line.top,
           "recent rows follow their heading and the bookmark heading follows them");
    expect(!layout.empty_note.has_value() && layout.start_heading.bottom <= rows.at(3).line.top &&
               layout.key_heading.top >= rows.at(5).line.bottom,
           "the start rows sit under their heading and above the key heading");
    expect(rows.at(3).label.left == rows.at(0).label.left &&
               rows.at(3).label.right == rows.at(0).label.right &&
               layout.keys.at(0).key.left == rows.at(0).label.left,
           "path rows, start rows and the shortcut keys share one left axis");
}

// 列の中央・幅・行の高さと並び・見出しの有無・鍵の行を、DPI ごとに確かめる。
void verify_layout_at(std::uint32_t dpi)
{
    const auto body = wide_body(dpi);
    const auto key_width = core::to_pixels(90, dpi);
    const auto placed =
        core::start_screen_layout(body, dpi, core::StartScreenShape{2, 1, false}, key_width);
    expect(placed.has_value(), "a wide body places the start screen");
    if (!placed.has_value())
    {
        return;
    }
    const auto &layout = placed.value();
    verify_column(layout, body, dpi);
    verify_rows(layout, body);
    expect(layout.keys.at(0).key.top >= layout.key_heading.bottom &&
               core::width_of(layout.keys.at(0).key) == key_width &&
               layout.keys.at(0).label.left > layout.keys.at(0).key.right &&
               layout.keys.at(2).key.bottom == layout.column.bottom,
           "the key rows use the measured key width in one column");
}

void verify_narrow_layouts()
{
    const auto body =
        core::body_layout_in(core::LayoutRect{0, 40, 400, 800}, 96, core::default_font_size());
    const auto narrow = core::start_screen_layout(body, 96, core::StartScreenShape{}, 0);
    expect(narrow.has_value() &&
               core::width_of(narrow.value().column) ==
                   core::width_of(body.content) - (2 * core::start_column_margin_dips),
           "a narrow body keeps 16 DIP on both sides of the column");
    expect(narrow.has_value() && narrow.value().rows.size() == 3 &&
               !narrow.value().recent_heading.has_value() &&
               !narrow.value().bookmark_heading.has_value() &&
               core::width_of(narrow.value().keys.at(0).key) == 64,
           "without lists only the start rows are placed and the key column is 64 DIP");
    const auto tiny =
        core::body_layout_in(core::LayoutRect{0, 40, 90, 800}, 96, core::default_font_size());
    expect(!core::start_screen_layout(tiny, 96, core::StartScreenShape{}, 0).has_value(),
           "a body too narrow for the column places nothing");
    const auto empty = core::start_screen_layout(body, 96, core::StartScreenShape{0, 0, true}, 0);
    expect(empty.has_value() && empty.value().empty_note.has_value() &&
               empty.value().empty_note.value().bottom <= empty.value().start_heading.top,
           "the empty note sits above the start heading");
}

void verify_shape()
{
    const auto entries = core::start_entries(paths_named("recent", 2), paths_named("mark", 1));
    const auto shape = core::start_screen_shape(entries, true);
    expect(shape.recent == 2 && shape.bookmarks == 1 && !shape.empty_note,
           "the shape counts the recent and bookmark rows");
    const auto none = core::start_entries({}, {});
    expect(core::start_screen_shape(none, true).empty_note &&
               !core::start_screen_shape(none, false).empty_note,
           "the empty note shows only after the lists were read");
}

// 当たりは行の中だけ（見出し・題・鍵の行・列の左右・帯の下は外れ）。
void verify_hits()
{
    const auto body = wide_body(96);
    const auto placed = core::start_screen_layout(body, 96, core::StartScreenShape{2, 1, false}, 0);
    if (!placed.has_value())
    {
        expect(false, "the hit layout is placed");
        return;
    }
    const auto &layout = placed.value();
    bool every = true;
    for (std::size_t index = 0; index < layout.rows.size(); ++index)
    {
        const auto [x, y] = middle_of(layout.rows.at(index).line);
        every = every && core::start_screen_row_at(layout, body, x, y) == std::optional{index};
    }
    expect(every, "the middle of each row hits that row");
    const auto [heading_x, heading_y] = middle_of(layout.start_heading);
    const auto [key_x, key_y] = middle_of(layout.keys.at(1).label);
    const auto [title_x, title_y] = middle_of(layout.title);
    expect(!core::start_screen_row_at(layout, body, heading_x, heading_y).has_value() &&
               !core::start_screen_row_at(layout, body, key_x, key_y).has_value() &&
               !core::start_screen_row_at(layout, body, title_x, title_y).has_value(),
           "headings, keys and the title are not selectable");
    const auto [row_x, row_y] = middle_of(layout.rows.at(0).line);
    expect(!core::start_screen_row_at(layout, body, layout.column.left - 1, row_y).has_value() &&
               !core::start_screen_row_at(layout, body, layout.column.right, row_y).has_value() &&
               !core::start_screen_row_at(layout, body, row_x, body.band.bottom).has_value(),
           "points left or right of the column or under the band miss");
}

void verify_layout()
{
    verify_layout_at(96);
    verify_layout_at(144);
    verify_layout_at(192);
    verify_narrow_layouts();
    verify_shape();
    verify_hits();
}

// 窓と同じ押しと離しの道（core::start_screen_layout と start_screen_row_at・start_pressed・
// start_released）。点 (x, y) は 96 DPI の広い本文の上。離しが行を選べば ChooseStartEntry を送る。
[[nodiscard]] std::optional<std::size_t> row_under(app::EditorController &controller,
                                                   std::pair<std::int32_t, std::int32_t> point)
{
    const auto view = shown(controller);
    if (!view.has_value())
    {
        return std::nullopt;
    }
    const auto body = wide_body(96);
    const auto layout = core::start_screen_layout(
        body, 96, core::start_screen_shape(view.value().entries, view.value().lists_read), 0);
    return layout.has_value()
               ? core::start_screen_row_at(layout.value(), body, point.first, point.second)
               : std::nullopt;
}

[[nodiscard]] std::optional<core::StartPress> press_at(app::EditorController &controller,
                                                       std::pair<std::int32_t, std::int32_t> point)
{
    const auto view = shown(controller);
    return view.has_value()
               ? core::start_pressed(view.value().entries, row_under(controller, point))
               : std::nullopt;
}

void release_at(app::EditorController &controller, const std::optional<core::StartPress> &press,
                std::pair<std::int32_t, std::int32_t> point)
{
    const auto view = shown(controller);
    if (!view.has_value())
    {
        return;
    }
    const auto chosen =
        core::start_released(press, view.value().entries, row_under(controller, point));
    if (chosen.has_value())
    {
        applied(controller, app::ChooseStartEntry{chosen.value()});
    }
}

// レビュー M3 の反例 1: 「新しいファイル」を押したまま履歴の読みが届き、同じ点で離す。読みの後は
// 同じ点の下が最近の 1 件目（同じ番号 0）になるが、押した行と中身が違うので何も開かない。
void verify_press_across_read()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\work\\recent0.txt", Bytes{std::string("zero")});
    applied(controller, app::FirstFramePresented{});
    const auto before = core::start_screen_layout(wide_body(96), 96, core::StartScreenShape{}, 0);
    if (!before.has_value())
    {
        expect(false, "the first frame places the start screen");
        return;
    }
    const auto point = middle_of(before.value().rows.at(0).line);
    const auto press = press_at(controller, point);
    expect(press.has_value() && press.value().entry.kind == core::StartEntryKind::new_file,
           "the press lands on the new-file row");
    editing.start_lists().serve(app::StartLists{paths_named("recent", 2), {}});
    applied(controller, app::WorkCompleted{});
    expect(row_under(controller, point) == std::optional<std::size_t>{0},
           "after the read the same point is over the first recent row");
    release_at(controller, press, point);
    expect(shown(controller).has_value() && !controller.frame().document.path.has_value(),
           "a press that the read moved under another row opens nothing");
}

// レビュー M3 の反例 2: 3 件目を押したまま、2 件目が開けずに行から外れ、同じ点で離す。同じ番号の
// 行は元の 4 件目だが、押した行と中身が違うので何も開かない。押したままの行で離せば開く。
void verify_press_across_forget()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\work\\recent2.txt", Bytes{std::string("two")});
    editing.files().hold_at("C:\\work\\recent3.txt", Bytes{std::string("three")});
    editing.history().serve(app::FileHistory{paths_named("recent", 4)});
    present_with(editing, app::StartLists{paths_named("recent", 4), {}});
    const auto layout =
        core::start_screen_layout(wide_body(96), 96, core::StartScreenShape{4, 0, false}, 0);
    if (!layout.has_value())
    {
        expect(false, "the read lists place the start screen");
        return;
    }
    const auto point = middle_of(layout.value().rows.at(2).line);
    const auto press = press_at(controller, point);
    applied(controller, app::ChooseStartEntry{1});
    const auto dropped = shown(controller);
    expect(dropped.has_value() && dropped.value().entries.size() == 3 + 3,
           "the missing second file drops out of the rows");
    release_at(controller, press, point);
    expect(shown(controller).has_value() && !controller.frame().document.path.has_value(),
           "a press whose row was dropped before the release opens nothing");
    const auto again = press_at(controller, point);
    release_at(controller, again, point);
    expect(controller.frame().document.path == path_named("recent3.txt"),
           "a press and a release on the same row still open it");
}
} // namespace

void verify_start_screen_scope()
{
    verify_layout();
    verify_press_across_read();
    verify_press_across_forget();
    verify_selection_steps();
    verify_first_frame_and_one_read();
    verify_ordinary_selection();
    verify_running_rows();
    verify_failed_rows();
    verify_background_rows();
    verify_start_rows();
    verify_conditions();
    verify_tabs_and_quiet_start();
    verify_vim();
}
} // namespace nenenib::tests
