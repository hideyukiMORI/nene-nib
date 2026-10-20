// scope `--split` の単体テスト（ADR 0042 決定 2・ADR 0148）。枠の配置と補正の純関数（決定
// 1・9）と、 分ける・移る・閉じる・送るの状態と意図と frame（決定 2〜5・8〜10・12）の契約。
#include "BodyLayout.hpp"
#include "CaretAffinity.hpp"
#include "CaretMotion.hpp"
#include "ClosePane.hpp"
#include "CloseTab.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DevicePixels.hpp"
#include "DisplayForm.hpp"
#include "Edit.hpp"
#include "EditAdjust.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EndSession.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "GiantLine.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PaneLayouts.hpp"
#include "PaneSide.hpp"
#include "PlaceCaret.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSession.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchPane.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimKey.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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
using app::EditorFrame;
using core::PaneSide;
using core::SplitLayout;

constexpr std::string_view six_lines = "l1\nl2\nl3\nl4\nl5\nl6";

[[nodiscard]] core::TextPosition at(std::size_t line, std::size_t column)
{
    return core::TextPosition{core::LineNumber{line}, core::Column{column}};
}

[[nodiscard]] app::PlaceCaret
place(std::size_t line, std::size_t column,
      core::SelectionAnchoring anchoring = core::SelectionAnchoring::collapse)
{
    return app::PlaceCaret{at(line, column), anchoring};
}

[[nodiscard]] bool other_caret_at(const EditorFrame &frame, std::size_t line, std::size_t column)
{
    return frame.other.has_value() && frame.other.value().caret.position.line.value == line &&
           frame.other.value().caret.position.column.value == column;
}

[[nodiscard]] std::size_t other_first(const EditorFrame &frame)
{
    return frame.other.has_value() ? frame.other.value().first_visible.value : 0;
}

[[nodiscard]] std::string joined_lines(const std::vector<app::LineView> &lines)
{
    std::string joined;
    for (const auto &line : lines)
    {
        joined += joined.empty() ? line.text : "|" + line.text;
    }
    return joined;
}

[[nodiscard]] std::string other_lines(const EditorFrame &frame)
{
    return frame.other.has_value() ? joined_lines(frame.other.value().lines) : "<none>";
}

[[nodiscard]] std::string other_title(const EditorFrame &frame)
{
    return frame.other.has_value() ? std::string(frame.other.value().document.title.text())
                                   : "<none>";
}

[[nodiscard]] std::size_t other_total(const EditorFrame &frame)
{
    return frame.other.has_value() ? frame.other.value().total_lines : 0;
}

// もう 1 つの枠が表示形式 display で、どの行にも選択が無いか。
[[nodiscard]] bool other_plain(const EditorFrame &frame, core::DisplayForm display)
{
    if (!frame.other.has_value() || frame.other.value().display != display)
    {
        return false;
    }
    for (const auto &line : frame.other.value().lines)
    {
        if (line.selection.presence == core::SelectionPresence::present)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string message_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] bool nothing_selected(const EditorFrame &frame)
{
    for (const auto &line : frame.lines)
    {
        if (line.selection.presence == core::SelectionPresence::present)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] core::FilePath work_path(std::string_view name)
{
    return core::FilePath::parse("C:\\work\\" + std::string(name) + ".txt").value();
}

void hold_work(ScriptedFiles &files)
{
    for (const std::string_view name : {"a", "b", "c"})
    {
        files.hold_at("C:\\work\\" + std::string(name) + ".txt", Bytes{std::string(name)});
    }
}

// 本文が l1〜l6 の 1 本のタブ。見えている行は 3 行で、キャレットは末尾（6 行目の 3 桁目）・先頭行は
// 4。
void type_six_lines(Editing &editing)
{
    applied(editing.controller(), app::VisibleLines{3});
    applied(editing.controller(), app::InsertText{std::string(six_lines)});
}

// ---------------------------------------------------------------- 純関数（決定 1・9）

void verify_pane_layouts_none()
{
    for (const auto dpi : {96U, 120U, 144U, 192U})
    {
        const auto whole = core::body_layout(800, 600, dpi, core::default_font_size());
        const auto panes = core::pane_layouts(core::LayoutRect{0, 0, 800, 600}, dpi,
                                              core::default_font_size(), SplitLayout::none);
        expect(panes.first.band == whole.band && panes.first.content == whole.content &&
                   panes.first.gutter == whole.gutter &&
                   panes.first.visible_lines == whole.visible_lines && !panes.second.has_value() &&
                   !panes.divider.has_value(),
               "one pane is the body layout itself");
    }
}

void check_vertical(std::int32_t width, std::uint32_t dpi)
{
    const auto whole = core::body_layout(width, 600, dpi, core::default_font_size());
    const auto panes = core::pane_layouts(core::LayoutRect{0, 0, width, 600}, dpi,
                                          core::default_font_size(), SplitLayout::vertical);
    expect(panes.second.has_value() && panes.divider.has_value(),
           "a vertical split has two panes and a divider");
    if (!panes.second.has_value() || !panes.divider.has_value())
    {
        return;
    }
    const auto &left = panes.first;
    const auto &right = panes.second.value();
    const auto &divider = panes.divider.value();
    const auto line = std::max(core::to_pixels(1, dpi), 1);
    expect(core::width_of(left.content) == core::width_of(right.content) &&
               core::height_of(left.content) == core::height_of(right.content) &&
               core::width_of(left.band) == core::width_of(right.band),
           "the first and the second pane of a vertical split have the same content size");
    expect(left.band.right <= right.band.left, "the first pane is on the left of the second");
    expect(left.band.left == 0 && right.band.right == width && divider.left == left.band.right &&
               divider.right == right.band.left,
           "the panes and the divider cover the band from edge to edge");
    expect(core::width_of(divider) == line || core::width_of(divider) == line + 1,
           "the divider is one DIP and takes the odd pixel");
    expect(left.band.top == whole.band.top && left.band.bottom == whole.band.bottom &&
               left.visible_lines == whole.visible_lines &&
               right.visible_lines == whole.visible_lines,
           "a vertical split keeps the height and the visible lines");
    expect(left.gutter.left == left.band.left && right.gutter.left == right.band.left,
           "each pane has its own gutter at its left edge");
}

void verify_pane_layouts_vertical()
{
    for (const auto width : {640, 641, 1001, 1366})
    {
        for (const auto dpi : {96U, 120U, 144U, 192U})
        {
            check_vertical(width, dpi);
        }
    }
}

// 操作中の枠ともう 1 つの枠の配置は側で引く（#683・描画と窓とマウスが同じ 1 本）。1 枠では
// どちらの側でも first で、もう 1 つの枠は無い。
void verify_pane_layout_sides()
{
    const core::LayoutRect client{0, 0, 1000, 700};
    const auto font = core::default_font_size();
    const auto one = core::pane_layouts(client, 96U, font, SplitLayout::none);
    expect(core::active_pane_layout(one, PaneSide::second).band == one.first.band &&
               !core::other_pane_layout(one, PaneSide::first).has_value(),
           "one pane: the operated pane is first on either side and there is no other pane");
    const auto two = core::pane_layouts(client, 96U, font, SplitLayout::vertical);
    const auto other_of_first = core::other_pane_layout(two, PaneSide::first);
    const auto other_of_second = core::other_pane_layout(two, PaneSide::second);
    expect(
        two.second.has_value() &&
            core::active_pane_layout(two, PaneSide::first).band == two.first.band &&
            core::active_pane_layout(two, PaneSide::second).band == two.second.value().band &&
            other_of_first.has_value() && other_of_first.value().band == two.second.value().band &&
            other_of_second.has_value() && other_of_second.value().band == two.first.band,
        "two panes: the operated pane is on its side and the other pane is on the opposite side");
}

void check_horizontal(std::int32_t height, std::uint32_t dpi)
{
    const auto whole = core::body_layout(900, height, dpi, core::default_font_size());
    const auto panes = core::pane_layouts(core::LayoutRect{0, 0, 900, height}, dpi,
                                          core::default_font_size(), SplitLayout::horizontal);
    expect(panes.second.has_value() && panes.divider.has_value(),
           "a horizontal split has two panes and a divider");
    if (!panes.second.has_value() || !panes.divider.has_value())
    {
        return;
    }
    const auto &top = panes.first;
    const auto &bottom = panes.second.value();
    const auto &divider = panes.divider.value();
    const auto line = std::max(core::to_pixels(1, dpi), 1);
    expect(core::height_of(top.band) == core::height_of(bottom.band) &&
               core::width_of(top.content) == core::width_of(bottom.content) &&
               core::height_of(top.content) == core::height_of(bottom.content) &&
               core::width_of(top.content) == core::width_of(whole.content),
           "the first and the second pane of a horizontal split have the same content size and "
           "the full width");
    expect(top.band.bottom <= bottom.band.top, "the first pane is above the second");
    expect(top.band.top == whole.band.top && bottom.band.bottom == whole.band.bottom &&
               divider.top == top.band.bottom && divider.bottom == bottom.band.top &&
               (core::height_of(divider) == line || core::height_of(divider) == line + 1),
           "the panes and a one-DIP divider cover the band from top to bottom");
    expect(top.visible_lines == bottom.visible_lines, "both panes show the same number of lines");
}

void verify_pane_layouts_horizontal()
{
    for (const auto height : {480, 481, 999})
    {
        for (const auto dpi : {96U, 144U, 192U})
        {
            check_horizontal(height, dpi);
        }
    }
}

// 編集より前は不変・後はずらす・削除の中は at へ畳む・ちょうど at は挿入の後ろへ（決定 9）。
void verify_offset_after_edit()
{
    using Row = std::pair<std::size_t, std::size_t>;
    const core::Edit insert{core::Offset{5}, "", "xy", core::Offset{5}};
    for (const auto &[before, after] : std::array<Row, 3>{{{3, 3}, {5, 7}, {8, 10}}})
    {
        expect(core::offset_after_edit(core::Offset{before}, insert).value == after,
               "an insertion keeps earlier offsets and pushes the rest behind it");
        const core::Edit undone{insert.at, insert.inserted, insert.removed, insert.at};
        expect(core::offset_after_edit(core::Offset{after}, undone).value == before,
               "undoing an insertion brings the offset back");
    }
    const core::Edit removal{core::Offset{5}, "abc", "", core::Offset{5}};
    for (const auto &[before, after] :
         std::array<Row, 6>{{{4, 4}, {5, 5}, {6, 5}, {7, 5}, {8, 5}, {9, 6}}})
    {
        expect(core::offset_after_edit(core::Offset{before}, removal).value == after,
               "a removal folds offsets inside it to its start and pulls the rest");
    }
    const core::Edit replacement{core::Offset{5}, "abc", "Z", core::Offset{5}};
    for (const auto &[before, after] : std::array<Row, 4>{{{5, 6}, {6, 5}, {8, 6}, {10, 8}}})
    {
        expect(core::offset_after_edit(core::Offset{before}, replacement).value == after,
               "a replacement applies both rules in one edit");
    }
}

// ---------------------------------------------------------------- 状態と意図と frame

// 1. 分けると、もう 1 つの枠は同じ文書を同じキャレットとスクロールで出す（決定 1・2）。
void verify_split_shows_same_document()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    const EditorFrame single = controller.frame();
    expect(single.layout == SplitLayout::none && !single.other.has_value(),
           "the window starts with one pane");
    expect(controller.apply_frame(app::SplitPane{SplitLayout::none}).layout == SplitLayout::none,
           "splitting into none does nothing");
    const EditorFrame split = controller.apply_frame(app::SplitPane{SplitLayout::vertical});
    expect(split.layout == SplitLayout::vertical && split.other.has_value(),
           "SplitPane vertical shows two panes");
    expect(other_caret_at(split, 6, 3) && caret_at(split, 6, 3) &&
               other_first(split) == split.first_visible.value && split.first_visible.value == 4,
           "the other pane starts with the same caret and scroll");
    expect(other_lines(split) == joined_lines(split.lines) && other_lines(split) == "l4|l5|l6" &&
               other_total(split) == 6 && other_title(split) == split.document.title.text(),
           "the other pane shows the same document");
    const EditorFrame again = controller.apply_frame(app::SplitPane{SplitLayout::horizontal});
    expect(again.layout == SplitLayout::vertical && message_of(again) == "もう分かれています",
           "splitting a split window changes nothing and says so in one line");
    expect(message_of(controller.apply_frame(app::ScrollLines{0})).empty(),
           "the notice lasts for one intent");
    Editing other;
    type_six_lines(other);
    const EditorFrame horizontal =
        other.controller().apply_frame(app::SplitPane{SplitLayout::horizontal});
    expect(horizontal.layout == SplitLayout::horizontal && other_caret_at(horizontal, 6, 3),
           "SplitPane horizontal shows two panes the same way");
}

// 操作中の枠の側（決定 2・8）。分けると first、移るたびに反転、1 枠に戻すと first。
void verify_active_side()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    expect(controller.frame().active_side == PaneSide::first, "one pane is on the first side");
    const EditorFrame split = controller.apply_frame(app::SplitPane{SplitLayout::vertical});
    expect(split.active_side == PaneSide::first, "splitting operates the first (left) pane");
    expect(controller.apply_frame(app::SwitchPane{}).active_side == PaneSide::second,
           "switching panes operates the second (right) pane");
    expect(controller.apply_frame(app::SwitchPane{}).active_side == PaneSide::first,
           "switching again comes back to the first pane");
    applied(controller, app::SwitchPane{});
    const EditorFrame closed = controller.apply_frame(app::ClosePane{});
    expect(closed.layout == SplitLayout::none && closed.active_side == PaneSide::first,
           "closing the pane from the second side goes back to the first side");
    applied(controller, app::NewTab{});
    applied(controller, app::SplitPane{SplitLayout::horizontal});
    applied(controller, app::SwitchTab{0});
    const EditorFrame other = controller.apply_frame(app::SwitchPane{});
    expect(other.active_side == PaneSide::second && other.active_tab == 1,
           "switching to the other document flips the side too");
    const EditorFrame quit = run_ex(controller, "q");
    expect(quit.layout == SplitLayout::none && quit.active_side == PaneSide::first,
           ":q goes back to the first side");
}

// 2. 同じ文書の枠を移ると、選択はキャレットに畳まれ、表示の欄が入れ替わる（決定 3）。
void verify_switch_same_document()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, place(1, 1));
    const EditorFrame selected =
        controller.apply_frame(place(1, 3, core::SelectionAnchoring::extend));
    expect(selected.first_visible.value == 1 && !nothing_selected(selected) &&
               other_first(selected) == 4 && other_caret_at(selected, 6, 3),
           "the operated pane moves and selects while the other pane stays");
    const EditorFrame swapped = controller.apply_frame(app::SwitchPane{});
    expect(caret_at(swapped, 6, 3) && swapped.first_visible.value == 4 && nothing_selected(swapped),
           "switching panes takes the other pane's caret and scroll");
    expect(other_caret_at(swapped, 1, 3) && other_first(swapped) == 1 && swapped.active_tab == 0 &&
               swapped.tabs.size() == 1,
           "the left pane keeps its caret, folded from the selection");
    const EditorFrame back = controller.apply_frame(app::SwitchPane{});
    expect(caret_at(back, 1, 3) && back.first_visible.value == 1 && other_caret_at(back, 6, 3) &&
               other_first(back) == 4 && nothing_selected(back),
           "switching twice comes back with the selection folded to its caret");
}

void verify_switch_swaps_wanted_column()
{
    Editing editing;
    open_vim_document(editing, "one two\nthree");
    auto &controller = editing.controller();
    vim_replay(controller, "j$");
    const auto wanted = controller.vim_state().wanted_column;
    applied(controller, app::SplitPane{SplitLayout::horizontal});
    vim_replay(controller, "k0");
    const auto moved = controller.vim_state().wanted_column;
    expect(!(moved == wanted), "the operated pane moves its own wanted column");
    applied(controller, app::SwitchPane{});
    expect(controller.vim_state().wanted_column == wanted && caret_at(controller.frame(), 2, 5),
           "switching panes brings back that pane's wanted column");
    applied(controller, app::SwitchPane{});
    expect(controller.vim_state().wanted_column == moved && caret_at(controller.frame(), 1, 1),
           "and switching back brings back the other one");
}

// 3. 文書の選択は操作中の枠へ。もう 1 つの枠は動かない（決定 4）。
void verify_tabs_leave_other_pane()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    const EditorFrame added = controller.apply_frame(app::NewTab{});
    expect(added.active_tab == 1 && added.lines.size() == 1 && added.lines.front().text.empty() &&
               other_lines(added) == "a" && other_caret_at(added, 1, 2) &&
               other_title(added) == "● 無題",
           "a new tab goes to the operated pane and the other pane keeps the first document");
    applied(controller, app::InsertText{"b"});
    const EditorFrame switched = controller.apply_frame(app::SwitchTab{0});
    expect(switched.active_tab == 0 && joined_lines(switched.lines) == "a" &&
               other_lines(switched) == "a",
           "switching tabs changes only the operated pane (now the same document twice)");
    const EditorFrame walked = controller.apply_frame(app::WalkRecentTab{core::TabStep::next});
    expect(walked.active_tab == 1 && joined_lines(walked.lines) == "b" &&
               other_lines(walked) == "a",
           "walking the recent tabs changes only the operated pane too");
}

// 4. 違う文書の枠へ移ると、キャレットは束ではなく PaneView の値で、使った順も動く（決定 3）。
void verify_switch_other_document()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, place(2, 1));
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"b"});
    const EditorFrame before = controller.frame();
    expect(before.active_tab == 1 && other_caret_at(before, 6, 3) && other_first(before) == 4,
           "the other pane shows the first tab at its own caret");
    const EditorFrame swapped = controller.apply_frame(app::SwitchPane{});
    expect(swapped.active_tab == 0 && caret_at(swapped, 6, 3) && swapped.first_visible.value == 4,
           "switching to the other document takes the pane's caret, not the parked one");
    expect(other_lines(swapped) == "b" && other_caret_at(swapped, 1, 2) &&
               other_title(swapped) == "● 無題",
           "the document left behind becomes the other pane");
    const EditorFrame back = controller.apply_frame(app::SwitchPane{});
    expect(back.active_tab == 1 && caret_at(back, 1, 2) && other_caret_at(back, 6, 3),
           "switching back returns to the second document");
}

void verify_switch_other_document_touches_recency()
{
    Editing editing;
    hold_work(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    for (const std::string_view name : {"a", "b", "c"})
    {
        applied(controller, app::OpenDocument{work_path(name)});
    }
    applied(controller, app::SwitchTab{0});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::SwitchTab{2});
    applied(controller, app::SwitchPane{});
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    const auto &written = editing.session().written();
    expect(written.has_value() && written.value().tabs.size() == 3 &&
               written.value().tabs.at(0).recency == 0 && written.value().tabs.at(2).recency == 1 &&
               written.value().tabs.at(1).recency == 2,
           "switching panes to another document moves it to the front of the recent tabs");
}

// 5. 同じ文書の 2 枠で編集すると、もう 1 つの枠のキャレットが補正の表に従う（決定 9）。
void verify_edit_adjusts_other_caret()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{5});
    applied(controller, app::InsertText{"abc\ndef"});
    applied(controller, place(1, 2));
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, place(2, 1));
    expect(other_caret_at(controller.apply_frame(app::InsertText{"Q"}), 1, 2),
           "typing after the other caret leaves it");
    applied(controller, place(1, 1));
    expect(other_caret_at(controller.apply_frame(app::InsertText{"P"}), 1, 3),
           "typing before the other caret pushes it");
    applied(controller, place(1, 3));
    expect(other_caret_at(controller.apply_frame(app::InsertText{"R"}), 1, 4),
           "typing exactly at the other caret pushes it behind the typed text");
    expect(other_caret_at(controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo}),
                          1, 3),
           "undo brings the other caret back");
    expect(other_caret_at(controller.apply_frame(app::HistoryAction{core::HistoryDirection::redo}),
                          1, 4),
           "redo moves it again");
    applied(controller, place(1, 2));
    applied(controller, place(1, 5, core::SelectionAnchoring::extend));
    const EditorFrame removed =
        controller.apply_frame(app::DeleteText{core::DeleteDirection::backward});
    expect(joined_lines(removed.lines) == "Pc|Qdef" && other_caret_at(removed, 1, 2),
           "removing text around the other caret folds it to the start of the removal");
    const EditorFrame restored =
        controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(joined_lines(restored.lines) == "PaRbc|Qdef" && other_caret_at(restored, 1, 5),
           "undoing the removal puts the folded caret behind the restored text");
}

void verify_edit_adjusts_other_scroll()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::horizontal});
    applied(controller, place(1, 1));
    const EditorFrame opened = controller.apply_frame(app::InsertText{"x\n"});
    expect(other_caret_at(opened, 7, 3) && other_first(opened) == 5 &&
               other_lines(opened) == "l4|l5|l6",
           "a new line above shifts the other pane's caret and first line with its text");
    const EditorFrame undone =
        controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(other_caret_at(undone, 6, 3) && other_first(undone) == 4, "undo shifts them back");
    const EditorFrame redone =
        controller.apply_frame(app::HistoryAction{core::HistoryDirection::redo});
    expect(other_caret_at(redone, 7, 3) && other_first(redone) == 5, "redo shifts them again");
}

// 先頭行は「行頭の offset を offset_after_edit で写して行番号に戻す」の 1 つの規則（決定 9）。
void verify_edit_adjusts_other_first_line()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::horizontal});
    applied(controller, place(4, 1));
    const EditorFrame opened = controller.apply_frame(app::InsertText{"\n"});
    expect(other_first(opened) == 5 && other_lines(opened) == "l4|l5|l6",
           "a line feed at the start of the other pane's first line moves the first line with "
           "its text");
    applied(controller, place(5, 2));
    const EditorFrame typed = controller.apply_frame(app::InsertText{"z"});
    expect(other_first(typed) == 5 && other_lines(typed) == "lz4|l5|l6",
           "typing inside the first line keeps it");
    applied(controller, place(1, 1));
    applied(controller, place(2, 1, core::SelectionAnchoring::extend));
    const EditorFrame removed =
        controller.apply_frame(app::DeleteText{core::DeleteDirection::backward});
    expect(other_first(removed) == 4 && other_lines(removed) == "lz4|l5|l6",
           "removing a line above the first line pulls it up by one");
}

// 矩形の編集は 1 つの置換なので、矩形の中にあったもう 1 つの枠のキャレットは先頭へ畳まれる。
void verify_block_edit_folds_other_caret()
{
    Editing editing;
    open_vim_document(editing, "abcd\nefgh\nijkl");
    auto &controller = editing.controller();
    vim_replay(controller, "jl");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    vim_replay(controller, "gg<C-v>jld");
    const EditorFrame removed = controller.frame();
    expect(joined_lines(removed.lines) == "cd|gh|ijkl" && other_caret_at(removed, 1, 1),
           "a block removal folds the other caret inside the block to its start");
    Editing after;
    open_vim_document(after, "abcd\nefgh\nijkl");
    auto &behind = after.controller();
    vim_replay(behind, "jj");
    applied(behind, app::SplitPane{SplitLayout::vertical});
    vim_replay(behind, "gg<C-v>jld");
    expect(other_caret_at(behind.frame(), 3, 1),
           "a block removal pulls an other caret behind the block");
}

// 6. 違う文書を出しているもう 1 つの枠は、操作中の枠の編集で動かない。
void verify_edit_leaves_other_document()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"x\ny\nz\n"});
    const EditorFrame typed = controller.apply_frame(app::InsertText{"more"});
    expect(other_caret_at(typed, 6, 3) && other_first(typed) == 4 &&
               other_lines(typed) == "l4|l5|l6",
           "editing another document never moves the other pane");
}

// 本文が a b c d の 4 本のタブ。アクティブは右端の d。
void open_four_tabs(app::EditorController &controller)
{
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    for (const std::string_view body : {"b", "c", "d"})
    {
        applied(controller, app::NewTab{});
        applied(controller, app::InsertText{std::string(body)});
    }
}

// 7. 文書を閉じる（決定 5）。
void verify_close_tab_redirects_other()
{
    Editing editing;
    auto &controller = editing.controller();
    open_four_tabs(controller);
    applied(controller, app::SwitchTab{1});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::SwitchTab{3});
    const EditorFrame parked = controller.apply_frame(app::CloseTab{1});
    expect(parked.tabs.size() == 3 && joined_lines(parked.lines) == "d" &&
               other_lines(parked) == "d" && other_caret_at(parked, 1, 1),
           "closing the parked tab the other pane shows moves it to the top of the operated "
           "document");
    Editing second;
    auto &next = second.controller();
    open_four_tabs(next);
    applied(next, app::SwitchTab{2});
    applied(next, app::SplitPane{SplitLayout::vertical});
    applied(next, app::SwitchTab{3});
    const EditorFrame active = next.apply_frame(app::CloseTab{3});
    expect(active.active_tab == 2 && joined_lines(active.lines) == "c" &&
               other_lines(active) == "c" && other_caret_at(active, 1, 2),
           "closing the operated tab moves to its neighbour and leaves the other pane");
    const EditorFrame same = next.apply_frame(app::CloseTab{2});
    expect(same.active_tab == 1 && joined_lines(same.lines) == "b" && other_lines(same) == "b" &&
               other_caret_at(same, 1, 1),
           "closing the document both panes show moves the other pane with the neighbour");
    Editing last;
    applied(last.controller(), app::SplitPane{SplitLayout::vertical});
    const EditorFrame closing = last.controller().apply_frame(app::CloseTab{0});
    expect(closing.closing && closing.tabs.size() == 1,
           "closing the last tab still asks the window to close (D22)");
}

// 8. 枠を閉じる・Ex の `:q` 系と `:only`（決定 5）。
void verify_close_pane()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, place(2, 2));
    const EditorFrame before = controller.frame();
    const EditorFrame closed = controller.apply_frame(app::ClosePane{});
    expect(closed.layout == SplitLayout::none && !closed.other.has_value() &&
               caret_at(closed, 2, 2) && closed.first_visible == before.first_visible &&
               joined_lines(closed.lines) == joined_lines(before.lines),
           "ClosePane goes back to one pane and leaves the operated pane as it was");
    expect(controller.apply_frame(app::ClosePane{}).layout == SplitLayout::none,
           "ClosePane on one pane does nothing");
}

void verify_ex_quit_closes_pane()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    vim_replay(controller, "x");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    vim_replay(controller, "j");
    const EditorFrame quit = run_ex(controller, "q");
    expect(quit.layout == SplitLayout::none && !quit.closing && quit.tabs.size() == 1 &&
               quit.document.save_state == core::SaveState::modified &&
               !message_of(quit).starts_with("E37"),
           ":q on two panes closes the operated pane and keeps the unsaved document");
    expect(caret_at(quit, 1, 1), "the remaining pane is the one that was not operated");
    const EditorFrame refused = run_ex(controller, "q");
    expect(refused.layout == SplitLayout::none && message_of(refused).starts_with("E37") &&
               !refused.closing,
           ":q on one pane still refuses an unsaved document");
    applied(controller, app::SplitPane{SplitLayout::horizontal});
    const EditorFrame written = run_ex(controller, "wq");
    expect(written.layout == SplitLayout::none && !written.closing &&
               editing.files().written_path() == "C:\\work\\note.txt" &&
               written.document.save_state == core::SaveState::saved,
           ":wq on two panes writes and then closes the pane");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    const EditorFrame only = run_ex(controller, "only");
    expect(only.layout == SplitLayout::none && !only.other.has_value() && only.tabs.size() == 1,
           ":only closes the other pane");
    expect(run_ex(controller, "on").layout == SplitLayout::none &&
               message_of(run_ex(controller, "only!")) != "only",
           ":on is the shortest form and a bang is not accepted");
}

void verify_ex_quit_force_and_exit()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    vim_replay(controller, "x");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    const EditorFrame forced = run_ex(controller, "q!");
    expect(forced.layout == SplitLayout::none && !forced.closing && forced.tabs.size() == 1 &&
               forced.document.save_state == core::SaveState::modified &&
               !message_of(forced).starts_with("E37"),
           ":q! on two panes closes the pane and keeps the unsaved document");
    applied(controller, app::SplitPane{SplitLayout::horizontal});
    const EditorFrame exited = run_ex(controller, "x");
    expect(exited.layout == SplitLayout::none && !exited.closing && exited.tabs.size() == 1 &&
               editing.files().written_path() == "C:\\work\\note.txt" &&
               exited.document.save_state == core::SaveState::saved,
           ":x on two panes writes the unsaved document and then closes the pane");
    vim_replay(controller, "x");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::ClosePane{});
    const EditorFrame refused = run_ex(controller, "q");
    expect(refused.layout == SplitLayout::none && message_of(refused).starts_with("E37") &&
               !refused.closing,
           ":q after going back to one pane takes the E37 path again");
}

void verify_ex_quit_brings_other_document()
{
    Editing editing;
    open_vim_document(editing, "one");
    auto &controller = editing.controller();
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::NewTab{});
    const EditorFrame quit = run_ex(controller, "q");
    expect(quit.layout == SplitLayout::none && quit.active_tab == 0 && quit.tabs.size() == 2 &&
               joined_lines(quit.lines) == "one",
           ":q on the operated pane leaves the other pane's document operated");
}

// 9. もう 1 つの枠だけを送る（決定 11）。収め方は操作中の枠と同じ。
void verify_scroll_pane()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::InsertText{"1\n2\n3\n4\n5\n6\n7\n8\n9\n10"});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, place(1, 1));
    const EditorFrame up = controller.apply_frame(app::ScrollPane{-3});
    expect(other_first(up) == 5 && up.first_visible.value == 1 && caret_at(up, 1, 1),
           "ScrollPane moves only the other pane");
    expect(other_first(controller.apply_frame(app::ScrollPane{-10})) == 1,
           "the other pane stops at the first line");
    const EditorFrame down = controller.apply_frame(app::ScrollPane{100});
    const EditorFrame active = controller.apply_frame(app::ScrollLines{100});
    expect(other_first(down) == active.first_visible.value && other_first(down) <= 10,
           "the other pane stops where the operated pane stops");
}

// 10. frame はもう 1 つの枠の見えている行とキャレットを載せる（決定 8）。格子も。
void verify_frame_other_pane()
{
    Editing editing;
    auto &controller = editing.controller();
    type_six_lines(editing);
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, place(1, 1));
    const EditorFrame frame = controller.apply_frame(app::ScrollPane{-1});
    expect(frame.layout == SplitLayout::vertical && other_lines(frame) == "l3|l4|l5" &&
               other_first(frame) == 3 && other_caret_at(frame, 6, 3) &&
               joined_lines(frame.lines) == "l1|l2|l3" && caret_at(frame, 1, 1),
           "the frame carries each pane's own lines and caret");
    expect(other_plain(frame, core::DisplayForm::text) && other_total(frame) == 6,
           "the other pane has no selection and its own display");
    Editing grid;
    auto &gridded = grid.controller();
    applied(gridded, app::VisibleLines{10, core::TextViewGeometry{85, 20, 96}});
    applied(gridded, app::InsertText{"abc\ndef"});
    applied(gridded, app::SelectDisplay{core::DisplayForm::grid});
    const EditorFrame cells = gridded.apply_frame(app::SplitPane{SplitLayout::horizontal});
    expect(cells.other.has_value() && cells.other.value().display == core::DisplayForm::grid &&
               !cells.other.value().grid.empty() &&
               cells.other.value().grid.size() == cells.grid.size() &&
               cells.other.value().lines.empty(),
           "a grid document fills the other pane's grid rows");
}

// 問いの答えを待つ文書は、もう 1 つの枠でも本文を読まない（ADR 0140 の決定 2）。
void verify_other_pane_waits_for_display_answer()
{
    Editing editing;
    editing.files().hold_at(
        "C:\\work\\giant.txt",
        Bytes{"head\n" + std::string(core::giant_line_threshold + 1, 'x') + "\ntail"});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{85, 20, 96}});
    applied(controller, app::OpenDocument{work_path("giant")});
    const EditorFrame split = controller.apply_frame(app::SplitPane{SplitLayout::vertical});
    expect(split.display_pending && split.other.has_value() && split.other.value().lines.empty() &&
               split.other.value().grid.empty(),
           "the other pane of a document waiting for the display answer has no lines nor grid");
}

// 無題の未変更タブへ開くと本文が丸ごと替わるので、同じタブを見るもう 1 つの枠は先頭に戻る（決定
// 5）。
void verify_open_into_blank_restarts_other()
{
    Editing editing;
    hold_work(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_replay(controller, "$");
    expect(controller.vim_state().wanted_column.has_value(), "$ sets the wanted column");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    const EditorFrame opened = controller.apply_frame(app::OpenDocument{work_path("a")});
    expect(opened.tabs.size() == 1 && other_lines(opened) == "a" && other_caret_at(opened, 1, 1) &&
               other_first(opened) == 1,
           "opening into the blank tab shows the new text from its top in the other pane too");
    applied(controller, app::SwitchPane{});
    expect(!controller.vim_state().wanted_column.has_value(),
           "the other pane starts without a wanted column");
}

// 11. もう 1 つの枠はまだ読んでいない文書を指さない（決定 2・5）。
void verify_other_never_unloaded()
{
    const auto listed = [](std::string_view name, std::size_t rank)
    { return app::SessionTab{work_path(name), at(1, 1), core::LineNumber{1}, rank}; };
    Editing editing(SessionReading{std::optional<app::Session>{
                        app::Session{{listed("a", 0), listed("b", 1), listed("c", 2)}, 0}}},
                    [](ScriptedFiles &files)
                    {
                        hold_work(files);
                        files.hold_at("C:\\work\\b.txt",
                                      Bytes{std::unexpected(app::FileFailure::not_found)});
                    });
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    const EditorFrame unreadable = controller.apply_frame(app::SwitchTab{1});
    expect(unreadable.tabs.size() == 2 && unreadable.active_tab == 0 &&
               other_lines(unreadable) == "a" && !message_of(unreadable).empty(),
           "an unreadable tab is dropped and the other pane keeps its document");
    applied(controller, app::SwitchTab{1});
    const EditorFrame closed = controller.apply_frame(app::CloseTab{0});
    expect(closed.tabs.size() == 1 && closed.layout == SplitLayout::vertical &&
               other_lines(closed) == "c" && joined_lines(closed.lines) == "c",
           "closing the shown document points the other pane at a loaded document");
    const EditorFrame swapped = controller.apply_frame(app::SwitchPane{});
    expect(swapped.other.has_value() && other_lines(swapped) == "c",
           "the other pane is always readable");
}

// 12. 分ける・同じ文書の枠を移る・閉じるの道はファイルにも履歴にも自動退避にも触れない。
void verify_split_quiet_paths()
{
    Editing editing;
    hold_work(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::OpenDocument{work_path("a")});
    const auto reads = editing.files().reads();
    const auto touches = editing.autosave().touches();
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::SwitchPane{});
    applied(controller, app::ScrollPane{1});
    applied(controller, app::ClosePane{});
    expect(editing.files().reads() == reads && editing.files().written_path().empty() &&
               editing.history().reads() == 0 && editing.history().writes() == 0 &&
               editing.autosave().touches() == touches && editing.folders().requests().empty(),
           "splitting, switching panes and closing a pane never touch a file, the history nor "
           "autosave");
}

// 13. 枠の意図は使う人の操作（歩きを終える）。もう 1 つの枠を送るホイールは横位置の狙いを保つ。
void verify_pane_intents_in_tables()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::NewTab{});
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    expect(controller.tab_walking(), "walking the recent tabs keeps the walk");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    expect(!controller.tab_walking(), "SplitPane ends the walk");
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, app::SwitchPane{});
    expect(!controller.tab_walking(), "SwitchPane ends the walk");
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, app::ScrollPane{1});
    expect(!controller.tab_walking(), "ScrollPane ends the walk");
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, app::ClosePane{});
    expect(!controller.tab_walking(), "ClosePane ends the walk");

    Editing moving;
    auto &ordinary = moving.controller();
    applied(ordinary, app::VisibleLines{10, core::TextViewGeometry{400, 20, 96, 8.0F}});
    applied(ordinary, app::InsertText{"abcdef\nab\nabcdef"});
    applied(ordinary, app::SplitPane{SplitLayout::vertical});
    applied(ordinary,
            app::MoveCaret{core::CaretMotion::previous_line, core::SelectionAnchoring::collapse});
    applied(ordinary, app::ScrollPane{0});
    const EditorFrame kept = ordinary.apply_frame(
        app::MoveCaret{core::CaretMotion::previous_line, core::SelectionAnchoring::collapse});
    expect(caret_at(kept, 1, 7), "ScrollPane keeps the wanted horizontal position");
}
// 14. Vim の `Ctrl-w` は枠の鍵と同じ意図（ADR 0148 の決定 6・7・#683）。2 打鍵目は 1 文字。
EditorFrame window_command(app::EditorController &controller, std::string_view second)
{
    static_cast<void>(controller.press_vim_key(core::VimKey{core::VimSpecialKey::control_w}));
    return controller.press_vim_keys(vim_keys_of(second));
}

void verify_vim_window_commands()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    const EditorFrame lone = window_command(controller, "w");
    expect(lone.layout == SplitLayout::none && !lone.other.has_value(),
           "Ctrl-w w on one pane does nothing");
    const EditorFrame kept = window_command(controller, "c");
    expect(kept.layout == SplitLayout::none && !kept.closing && kept.tabs.size() == 1,
           "Ctrl-w c on one pane does nothing (the window stays)");
    const EditorFrame vertical = window_command(controller, "v");
    expect(vertical.layout == SplitLayout::vertical && vertical.active_side == PaneSide::first &&
               vertical.other.has_value(),
           "Ctrl-w v splits left and right");
    expect(window_command(controller, "l").active_side == PaneSide::second &&
               window_command(controller, "k").active_side == PaneSide::first,
           "Ctrl-w l and Ctrl-w k both move to the only neighbour");
    const EditorFrame again = window_command(controller, "s");
    expect(again.layout == SplitLayout::vertical && message_of(again) == "もう分かれています",
           "Ctrl-w s on two panes keeps the panes and says so");
    applied(controller, place(2, 1));
    const EditorFrame closed = window_command(controller, "c");
    expect(closed.layout == SplitLayout::none && caret_at(closed, 1, 1) && !closed.closing,
           "Ctrl-w c closes the operated pane and leaves the other pane operated");
    const EditorFrame horizontal = window_command(controller, "s");
    applied(controller, place(2, 2));
    const EditorFrame only = window_command(controller, "o");
    expect(horizontal.layout == SplitLayout::horizontal && only.layout == SplitLayout::none &&
               caret_at(only, 2, 2),
           "Ctrl-w s splits top and bottom and Ctrl-w o closes the other pane");
}
} // namespace

void verify_split_contracts()
{
    verify_pane_layouts_none();
    verify_pane_layouts_vertical();
    verify_pane_layouts_horizontal();
    verify_pane_layout_sides();
    verify_offset_after_edit();
    verify_split_shows_same_document();
    verify_active_side();
    verify_switch_same_document();
    verify_switch_swaps_wanted_column();
    verify_tabs_leave_other_pane();
    verify_switch_other_document();
    verify_switch_other_document_touches_recency();
    verify_edit_adjusts_other_caret();
    verify_edit_adjusts_other_scroll();
    verify_edit_adjusts_other_first_line();
    verify_block_edit_folds_other_caret();
    verify_edit_leaves_other_document();
    verify_close_tab_redirects_other();
    verify_close_pane();
    verify_ex_quit_closes_pane();
    verify_ex_quit_force_and_exit();
    verify_ex_quit_brings_other_document();
    verify_scroll_pane();
    verify_frame_other_pane();
    verify_other_never_unloaded();
    verify_other_pane_waits_for_display_answer();
    verify_open_into_blank_restarts_other();
    verify_split_quiet_paths();
    verify_pane_intents_in_tables();
    verify_vim_window_commands();
}

void verify_split_scope()
{
    verify_split_contracts();
}
} // namespace nenenib::tests
