// scope `--edge-arrows` の単体テスト（#1386・ADR 0144 の追補 2026-11-10）。通常モードの上下の
// 移動（↑ ↓ PgUp PgDn）が本文の最初か最後の画面行で止まったら、本文の先頭か末尾へ着く。Vim の
// モードは変えない。
#include "CaretMotion.hpp"
#include "CommandText.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MoveCaret.hpp"
#include "OpenCommandPalette.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SubmitCommand.hpp"
#include "SwitchPane.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::CaretMotion;
using core::SelectionAnchoring;

// 台本の文字組みは 1 文字 10 画素。幅 40 は 1 画面行に 4 文字（折り返す）、幅 400 は 40 文字。
constexpr core::TextViewGeometry narrow{40, 20, 96};
constexpr core::TextViewGeometry wide{400, 20, 96};
// 格子の 1 画面行は 8 升（ApplicationTests の page_grid_geometry と同じ）。
constexpr core::TextViewGeometry grid_geometry{85, 20, 96};

[[nodiscard]] core::TextPosition at(std::size_t line, std::size_t column)
{
    return core::TextPosition{core::LineNumber{line}, core::Column{column}};
}

[[nodiscard]] app::PlaceCaret place(std::size_t line, std::size_t column,
                                    SelectionAnchoring anchoring = SelectionAnchoring::collapse)
{
    return app::PlaceCaret{at(line, column), anchoring};
}

[[nodiscard]] app::MoveCaret move(CaretMotion motion,
                                  SelectionAnchoring anchoring = SelectionAnchoring::collapse)
{
    return app::MoveCaret{motion, anchoring};
}

void prepare(Editing &editing, std::string text, core::TextViewGeometry geometry,
             std::size_t visible = 3)
{
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{visible, geometry});
    applied(controller, app::InsertText{std::move(text)});
}

[[nodiscard]] core::TextPosition caret_after(app::EditorController &controller,
                                             const app::MoveCaret &intent)
{
    return controller.apply_frame(intent).caret.position;
}

// 見えている行の選択を「行:始め-終わり」で並べる（無ければ空）。
[[nodiscard]] std::string selection_of(const app::EditorFrame &frame)
{
    std::string result;
    for (const auto &line : frame.lines)
    {
        if (line.selection.presence == core::SelectionPresence::present)
        {
            result += (result.empty() ? "" : "|") + std::to_string(line.number.value) + ":" +
                      std::to_string(line.selection.begin.value) + "-" +
                      std::to_string(line.selection.end.value);
        }
    }
    return result;
}

void set_nowrap(app::EditorController &controller)
{
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{":set nowrap"});
    applied(controller, app::SubmitCommand{});
}

// 1. 折り返しオン: 最初の画面行の ↑ は本文の先頭、最後の画面行の ↓ は本文の末尾。途中の画面行は
// 今までどおり 1 画面行ずつ。
void verify_wrapped_edges()
{
    Editing editing;
    prepare(editing, "abcdefghij", narrow);
    auto &controller = editing.controller();
    applied(controller, place(1, 3));
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 1),
           "Up on the first wrapped row lands at the start of the text");
    applied(controller, place(1, 2));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(1, 6),
           "Down on a middle wrapped row still moves one screen row");
    applied(controller, place(1, 10));
    const auto end = controller.apply_frame(move(CaretMotion::next_line));
    expect(end.caret.position == at(1, 11) && !end.layout_failure.has_value(),
           "Down on the last wrapped row lands at the end of the text");
}

// 2. 折り返しオフ: 先頭の行の ↑ と最後の行の ↓。
void verify_unwrapped_edges()
{
    Editing editing;
    prepare(editing, "abcdefgh\nab", wide);
    auto &controller = editing.controller();
    set_nowrap(controller);
    applied(controller, place(2, 2));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 3),
           "unwrapped Down on the last line lands at the end of the text");
    applied(controller, place(1, 4));
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 1),
           "unwrapped Up on the first line lands at the start of the text");
    expect(controller.frame().settings.wrapping == core::LineWrapping::no_wrap,
           "the unwrapped edges ran without wrapping");
}

// 3. Shift は同じ着地まで選択を伸ばし、選択があるときの Shift なしは畳んで着地する。
void verify_selection_edges()
{
    Editing editing;
    prepare(editing, "ab\ncdef", wide);
    auto &controller = editing.controller();
    applied(controller, place(2, 2));
    const auto extended =
        controller.apply_frame(move(CaretMotion::next_line, SelectionAnchoring::extend));
    expect(extended.caret.position == at(2, 5) && selection_of(extended) == "2:2-5",
           "Shift+Down on the last line extends the selection to the end of the text");
    applied(controller, place(1, 2));
    const auto upward =
        controller.apply_frame(move(CaretMotion::previous_line, SelectionAnchoring::extend));
    expect(upward.caret.position == at(1, 1) && selection_of(upward) == "1:1-2",
           "Shift+Up on the first line extends the selection to the start of the text");
    applied(controller, place(2, 1));
    applied(controller, place(2, 3, SelectionAnchoring::extend));
    const auto collapsed = controller.apply_frame(move(CaretMotion::next_line));
    expect(collapsed.caret.position == at(2, 5) && selection_of(collapsed).empty(),
           "Down with a selection on the last line collapses it and lands at the end");
}

// 4. もう端に居るときは何も変えない（選択も狙う横位置も画面の先頭も）。選択があって Shift なしなら
// 畳むだけ。
void verify_already_at_edge()
{
    Editing editing;
    prepare(editing, "abcdefgh\nab", wide);
    auto &controller = editing.controller();
    applied(controller, place(1, 7));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 3),
           "Down from a long line clamps to the end of the short last line");
    const auto still = controller.apply_frame(move(CaretMotion::next_line));
    expect(still.caret.position == at(2, 3) && selection_of(still).empty(),
           "Down at the end of the text changes nothing");
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 7),
           "Down at the end keeps the wanted x of the earlier Down");
    applied(controller, place(2, 1));
    applied(controller, place(2, 3, SelectionAnchoring::extend));
    const auto kept =
        controller.apply_frame(move(CaretMotion::next_line, SelectionAnchoring::extend));
    expect(kept.caret.position == at(2, 3) && selection_of(kept) == "2:1-3",
           "Shift+Down at the end keeps the selection as it is");
    const auto folded = controller.apply_frame(move(CaretMotion::next_line));
    expect(folded.caret.position == at(2, 3) && selection_of(folded).empty(),
           "Down at the end with a selection only collapses it");
    applied(controller, place(1, 1));
    const auto top = controller.apply_frame(move(CaretMotion::previous_line));
    expect(top.caret.position == at(1, 1) && top.first_visible == core::LineNumber{1},
           "Up at the start of the text changes nothing");
}

// 5. 端へ着いた後の狙う横位置は着いた位置の横位置（ブラウザの textarea と同じ）。
void verify_wanted_x_after_edge()
{
    Editing editing;
    prepare(editing, "abcdefghij\nabcd", wide);
    auto &controller = editing.controller();
    applied(controller, place(2, 2));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 5),
           "Down on the last line lands at its end");
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 5),
           "Up after landing at the end goes up from the landed column");
    Editing upper;
    prepare(upper, "abcd\nabcdefghij", wide);
    auto &first = upper.controller();
    applied(first, place(1, 3));
    expect(caret_after(first, move(CaretMotion::previous_line)) == at(1, 1),
           "Up on the first line lands at the start");
    expect(caret_after(first, move(CaretMotion::next_line)) == at(2, 1),
           "Down after landing at the start goes down from the first column");
}

// 6. PgDn / PgUp が端に当たったときも同じ着地。当たらなければ今までどおり 1 画面ぶん。
void verify_page_edges()
{
    Editing editing;
    std::string text = "ab";
    for (std::size_t line = 1; line < 10; ++line)
    {
        text += "\nab";
    }
    prepare(editing, text, wide);
    auto &controller = editing.controller();
    applied(controller, place(1, 2));
    expect(caret_after(controller, move(CaretMotion::page_down)) == at(4, 2),
           "PgDn away from the end moves a whole screen");
    applied(controller, place(9, 1));
    const auto end = controller.apply_frame(move(CaretMotion::page_down));
    expect(end.caret.position == at(10, 3) && end.first_visible == core::LineNumber{8},
           "PgDn on the last page lands at the end with the last screen filled");
    const auto again = controller.apply_frame(move(CaretMotion::page_down));
    expect(again.caret.position == at(10, 3) && again.first_visible == core::LineNumber{8},
           "PgDn at the end of the text changes nothing");
    expect(caret_after(controller, move(CaretMotion::page_up)) == at(7, 3),
           "PgUp after landing at the end goes up from the landed column");
    applied(controller, place(2, 2));
    const auto start =
        controller.apply_frame(move(CaretMotion::page_up, SelectionAnchoring::extend));
    expect(start.caret.position == at(1, 1) && start.first_visible == core::LineNumber{1} &&
               selection_of(start).starts_with("1:1-") && selection_of(start).ends_with("|2:1-2"),
           "Shift+PgUp on the first page extends the selection to the start of the text");
}

// 7. 空の文書・末尾が改行で終わる文書（最後の行が空）。
void verify_empty_and_trailing_newline()
{
    Editing empty;
    prepare(empty, "", wide);
    auto &blank = empty.controller();
    for (const auto motion : {CaretMotion::next_line, CaretMotion::previous_line,
                              CaretMotion::page_down, CaretMotion::page_up})
    {
        const auto frame = blank.apply_frame(move(motion));
        expect(frame.caret.position == at(1, 1) && !frame.layout_failure.has_value() &&
                   blank.text().size_bytes() == 0,
               "vertical moves in an empty document change nothing");
    }
    Editing trailing;
    prepare(trailing, "ab\ncd\n", wide);
    auto &controller = trailing.controller();
    applied(controller, place(2, 2));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(3, 1),
           "Down onto the empty last line is an ordinary move");
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(3, 1),
           "Down on the empty last line is already at the end");
    Editing wrapped;
    prepare(wrapped, "abcdefghij\n", narrow);
    auto &rows = wrapped.controller();
    applied(rows, place(1, 10));
    expect(caret_after(rows, move(CaretMotion::next_line)) == at(2, 1),
           "Down from the last wrapped row of a line ending in a newline enters the empty line");
    applied(rows, place(1, 6));
    expect(caret_after(rows, move(CaretMotion::page_down)) == at(2, 1),
           "PgDn past the empty last line lands at the end of the text");
}

// 8. 端の移動は undo の単位を作らない。
void verify_undo_untouched()
{
    Editing editing;
    prepare(editing, "ab\ncd", wide);
    auto &controller = editing.controller();
    applied(controller, place(2, 1));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 3),
           "Down lands at the end before undo");
    applied(controller, place(1, 2));
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 1),
           "Up lands at the start before undo");
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    expect(controller.text().size_bytes() == 0, "undo after the edge moves undoes the typing");
    applied(controller, app::HistoryAction{core::HistoryDirection::redo});
    expect(controller.text().size_bytes() == 5, "redo restores the typing");
}

// 9. 格子（巨大な行の表示）でも同じ。200 文字の 1 行は 8 升 × 25 画面行。
void verify_grid_edges()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3, grid_geometry});
    applied(controller, app::InsertText{std::string(200, 'a')});
    applied(controller, place(1, 3));
    applied(controller, app::SelectDisplay{core::DisplayForm::grid});
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 1),
           "grid Up on the first screen row lands at the start");
    applied(controller, place(1, 195));
    const auto end = controller.apply_frame(move(CaretMotion::next_line));
    expect(end.caret.position == at(1, 201) && end.display == core::DisplayForm::grid,
           "grid Down on the last screen row lands at the end");
    applied(controller, place(1, 180));
    expect(caret_after(controller, move(CaretMotion::page_down)) == at(1, 201),
           "grid PgDn on the last page lands at the end");
}

// 10. split の両方の枠で同じ。
void verify_split_edges()
{
    Editing editing;
    prepare(editing, "ab\ncd", wide);
    auto &controller = editing.controller();
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    applied(controller, place(2, 1));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 3),
           "Down on the last line lands at the end in the first pane");
    applied(controller, app::SwitchPane{});
    applied(controller, app::VisibleLines{3, wide});
    applied(controller, place(1, 2));
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 1),
           "Up on the first line lands at the start in the second pane");
    applied(controller, place(2, 1));
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 3),
           "Down on the last line lands at the end in the second pane");
}

// 11. Vim のモードの上下は変えない（矢印の MoveCaret も本文の端へ飛ばない）。
void verify_vim_unchanged()
{
    Editing editing;
    prepare(editing, "ab\ncd", wide);
    auto &controller = editing.controller();
    applied(controller, place(2, 1));
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    expect(caret_after(controller, move(CaretMotion::next_line)) == at(2, 1),
           "Vim Down on the last line does not jump to the end");
    applied(controller, place(1, 2));
    expect(caret_after(controller, move(CaretMotion::previous_line)) == at(1, 2),
           "Vim Up on the first line does not jump to the start");
}
} // namespace

void verify_edge_arrows_scope()
{
    verify_wrapped_edges();
    verify_unwrapped_edges();
    verify_selection_edges();
    verify_already_at_edge();
    verify_wanted_x_after_edge();
    verify_page_edges();
    verify_empty_and_trailing_newline();
    verify_undo_untouched();
    verify_grid_edges();
    verify_split_edges();
    verify_vim_unchanged();
}
} // namespace nenenib::tests
