// scope `--selection-arrows` の単体テスト（#1393・施主決定 D82・ADR 0009 の追補 2026-11-11）。
// 通常モードで選択があるときの Shift なしの移動は Windows の標準（メモ帳の RichEdit の実測）どおり
// 選択の端から動く: ← → は先頭 / 末尾へ畳むだけ、↑ Home PgUp は先頭の側、↓ End PgDn は末尾の側の
// 画面行から、上下の横位置はキャレット（動く端）のもの。Vim のモードと Shift つきは変えない。
#include "CaretAffinity.hpp"
#include "CaretMotion.hpp"
#include "DisplayForm.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MoveCaret.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
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
// 格子の 1 画面行は 8 升（EdgeArrowTests と同じ）。
constexpr core::TextViewGeometry grid_geometry{85, 20, 96};

// 3 行（11・13・11 文字）。
constexpr const char *three_lines = "foo bar baz\nline two here\nthird row x";

[[nodiscard]] core::TextPosition at(std::size_t line, std::size_t column)
{
    return core::TextPosition{core::LineNumber{line}, core::Column{column}};
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

// anchor から caret まで選ぶ（caret が動く端）。
void select(app::EditorController &controller, core::TextPosition anchor, core::TextPosition caret)
{
    applied(controller, app::PlaceCaret{anchor, SelectionAnchoring::collapse});
    applied(controller, app::PlaceCaret{caret, SelectionAnchoring::extend});
}

[[nodiscard]] bool selected(const app::EditorFrame &frame)
{
    for (const auto &line : frame.lines)
    {
        if (line.selection.presence == core::SelectionPresence::present)
        {
            return true;
        }
    }
    return false;
}

// 選択（錨, キャレット）。
using Span = std::pair<core::TextPosition, core::TextPosition>;

// 選んでから 1 鍵押したキャレット。着いたら選択は残らない。
[[nodiscard]] bool lands(app::EditorController &controller, const Span &span, CaretMotion motion,
                         core::TextPosition expected)
{
    select(controller, span.first, span.second);
    const auto frame = controller.apply_frame(move(motion));
    return frame.caret.position == expected && !selected(frame) &&
           !frame.layout_failure.has_value();
}

// 1. ← は選択の先頭、→ は末尾へ畳むだけで動かない（前向き・後ろ向きの両方）。
void verify_left_right_collapse()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    expect(lands(controller, {at(1, 5), at(2, 3)}, CaretMotion::previous_character, at(1, 5)),
           "Left with a forward selection collapses to its start without moving");
    expect(lands(controller, {at(1, 5), at(2, 3)}, CaretMotion::next_character, at(2, 3)),
           "Right with a forward selection collapses to its end without moving");
    expect(lands(controller, {at(2, 3), at(1, 5)}, CaretMotion::previous_character, at(1, 5)),
           "Left with a backward selection collapses to its start without moving");
    expect(lands(controller, {at(2, 3), at(1, 5)}, CaretMotion::next_character, at(2, 3)),
           "Right with a backward selection collapses to its end without moving");
    applied(controller, app::PlaceCaret{at(2, 3), SelectionAnchoring::collapse});
    expect(controller.apply_frame(move(CaretMotion::previous_character)).caret.position == at(2, 2),
           "Left without a selection still moves one character");
}

// 2. ↑ は先頭の行から、↓ は末尾の行から 1 行。横位置はキャレット（動く端）のもの。
void verify_up_down_origin()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    expect(lands(controller, {at(2, 3), at(3, 6)}, CaretMotion::previous_line, at(1, 6)),
           "Up with a forward selection goes up from its first line at the caret's column");
    expect(lands(controller, {at(3, 6), at(2, 3)}, CaretMotion::previous_line, at(1, 3)),
           "Up with a backward selection goes up from its first line at the caret's column");
    expect(lands(controller, {at(1, 3), at(2, 6)}, CaretMotion::next_line, at(3, 6)),
           "Down with a forward selection goes down from its last line at the caret's column");
    expect(lands(controller, {at(2, 6), at(1, 3)}, CaretMotion::next_line, at(3, 3)),
           "Down with a backward selection goes down from its last line at the caret's column");
}

// 3. Home は先頭の行の頭、End は末尾の行の末尾。
void verify_home_end_origin()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    expect(lands(controller, {at(1, 5), at(2, 6)}, CaretMotion::line_start, at(1, 1)),
           "Home with a forward selection goes to the start of its first line");
    expect(lands(controller, {at(2, 6), at(1, 5)}, CaretMotion::line_end, at(2, 14)),
           "End with a backward selection goes to the end of its last line");
    expect(lands(controller, {at(1, 5), at(2, 6)}, CaretMotion::line_end, at(2, 14)),
           "End with a forward selection goes to the end of its last line");
    expect(lands(controller, {at(2, 6), at(1, 5)}, CaretMotion::line_start, at(1, 1)),
           "Home with a backward selection goes to the start of its first line");
}

[[nodiscard]] std::string ten_lines()
{
    std::string text = "ab";
    for (std::size_t line = 1; line < 10; ++line)
    {
        text += "\nab";
    }
    return text;
}

// 4. PgUp / PgDn は ↑ ↓ と同じ起点（Issue の文のとおり・1 画面を超える本文の Windows の実測は
// まだ無い）。
void verify_page_origin()
{
    Editing editing;
    prepare(editing, ten_lines(), wide);
    auto &controller = editing.controller();
    expect(lands(controller, {at(4, 2), at(6, 2)}, CaretMotion::page_up, at(1, 2)),
           "PgUp with a forward selection goes a screen up from its first line");
    expect(lands(controller, {at(6, 3), at(4, 2)}, CaretMotion::page_down, at(9, 2)),
           "PgDn with a backward selection goes a screen down from its last line");
}

// 5. 端の行（#1386）と組み合わせる: 末尾が最後の行なら ↓ は本文の末尾、先頭が最初の行なら ↑ は
// 本文の先頭（キャレットがどちらの端でも）。
void verify_edges_combined()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    expect(lands(controller, {at(3, 3), at(2, 2)}, CaretMotion::next_line, at(3, 12)),
           "Down with a backward selection ending on the last line lands at the end of the text");
    expect(lands(controller, {at(2, 2), at(3, 3)}, CaretMotion::next_line, at(3, 12)),
           "Down with a forward selection ending on the last line lands at the end of the text");
    expect(lands(controller, {at(2, 4), at(1, 3)}, CaretMotion::previous_line, at(1, 1)),
           "Up with a selection starting on the first line lands at the start of the text");
    expect(lands(controller, {at(1, 3), at(2, 4)}, CaretMotion::previous_line, at(1, 1)),
           "Up with a forward selection starting on the first line lands at the start");
}

// 6. 折り返し（4 文字の画面行）: 起点は端の画面行、横位置はキャレットの x。末尾が折り返しの境目
// なら、選択を描く前の画面行から下へ。
void verify_wrapped_origin()
{
    Editing editing;
    prepare(editing, "abcdefghijklmnop", narrow);
    auto &controller = editing.controller();
    expect(lands(controller, {at(1, 6), at(1, 11)}, CaretMotion::previous_line, at(1, 3)),
           "wrapped Up goes up from the start's screen row at the caret's x");
    expect(lands(controller, {at(1, 11), at(1, 6)}, CaretMotion::next_line, at(1, 14)),
           "wrapped Down goes down from the end's screen row at the caret's x");
    expect(lands(controller, {at(1, 9), at(1, 3)}, CaretMotion::next_line, at(1, 11)),
           "wrapped Down from an end on a wrap boundary leaves the row the selection ends on");
    expect(lands(controller, {at(1, 7), at(1, 11)}, CaretMotion::line_start, at(1, 5)),
           "wrapped Home goes to the start of the start's screen row");
    expect(lands(controller, {at(1, 11), at(1, 6)}, CaretMotion::line_end, at(1, 13)),
           "wrapped End goes to the end of the end's screen row");
}

// 畳んだ後のキャレットの位置と画面行の側。
[[nodiscard]] bool rests(const app::EditorFrame &frame, core::TextPosition position,
                         core::CaretAffinity affinity)
{
    return frame.caret.position == position && frame.caret.affinity == affinity &&
           !selected(frame) && !frame.layout_failure.has_value();
}

// 6b. 折り返しの境目で ← → で畳んだキャレットは、選択を描いていた側の画面行に留まる（末尾へ畳めば
// 前の画面行の行末＝trailing・先頭へ畳めば後ろの画面行の行頭＝leading）。その後の ↓ ↑ もその画面行
// から動く（Windows の実測なし・設計席の決め・ADR 0009 の追補）。
void verify_wrapped_collapse_row()
{
    Editing editing;
    prepare(editing, "abcdefghijklmnop", narrow);
    auto &controller = editing.controller();
    // 前向き: Shift+End の末尾（1 つ目の画面行の右端）へ → で畳み、↓ は → を押さない ↓ と同じ所。
    applied(controller, app::PlaceCaret{at(1, 2), SelectionAnchoring::collapse});
    applied(controller, move(CaretMotion::line_end, SelectionAnchoring::extend));
    const auto direct = controller.apply_frame(move(CaretMotion::next_line));
    applied(controller, app::PlaceCaret{at(1, 2), SelectionAnchoring::collapse});
    applied(controller, move(CaretMotion::line_end, SelectionAnchoring::extend));
    expect(rests(controller.apply_frame(move(CaretMotion::next_character)), at(1, 5),
                 core::CaretAffinity::trailing),
           "wrapped Right collapses at the end of the screen row the selection was drawn on");
    expect(rests(controller.apply_frame(move(CaretMotion::next_line)), direct.caret.position,
                 direct.caret.affinity) &&
               direct.caret.position == at(1, 9) &&
               direct.caret.affinity == core::CaretAffinity::trailing,
           "wrapped Down after collapsing with Right leaves the selection's screen row");
    // 前向き: 2 つ目の画面行の右端へ畳んだ後の ↑ は 1 つ目の画面行の右端へ。
    applied(controller, app::PlaceCaret{at(1, 6), SelectionAnchoring::collapse});
    applied(controller, move(CaretMotion::line_end, SelectionAnchoring::extend));
    applied(controller, move(CaretMotion::next_character));
    expect(rests(controller.apply_frame(move(CaretMotion::previous_line)), at(1, 5),
                 core::CaretAffinity::trailing),
           "wrapped Up after collapsing with Right goes up from the selection's screen row");
    // 後ろ向き: 錨（末尾）が境目なら → は前の画面行の行末に畳み、↓ はそこから。
    select(controller, at(1, 9), at(1, 7));
    expect(rests(controller.apply_frame(move(CaretMotion::next_character)), at(1, 9),
                 core::CaretAffinity::trailing),
           "wrapped Right with a backward selection collapses on the row it was drawn on");
    expect(rests(controller.apply_frame(move(CaretMotion::next_line)), at(1, 13),
                 core::CaretAffinity::trailing),
           "wrapped Down after the backward collapse goes down from that row");
    // 前向き: 錨（先頭）が境目なら ← は後ろの画面行の行頭に畳み、↓ はそこから。
    select(controller, at(1, 5), at(1, 7));
    expect(rests(controller.apply_frame(move(CaretMotion::previous_character)), at(1, 5),
                 core::CaretAffinity::leading),
           "wrapped Left collapses at the head of the row the selection starts on");
    expect(rests(controller.apply_frame(move(CaretMotion::next_line)), at(1, 9),
                 core::CaretAffinity::leading),
           "wrapped Down after collapsing with Left goes down from that row");
}

// 7. 格子（巨大な行の表示・1 画面行 8 升）でも同じ起点。
void verify_grid_origin()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3, grid_geometry});
    applied(controller, app::InsertText{std::string(200, 'a')});
    applied(controller, app::PlaceCaret{at(1, 3), SelectionAnchoring::collapse});
    applied(controller, app::SelectDisplay{core::DisplayForm::grid});
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    expect(lands(controller, {at(1, 20), at(1, 3)}, CaretMotion::next_line, at(1, 27)),
           "grid Down goes down from the end's screen row at the caret's cell");
    expect(lands(controller, {at(1, 10), at(1, 20)}, CaretMotion::previous_line, at(1, 4)),
           "grid Up goes up from the start's screen row at the caret's cell");
    const auto frame = controller.frame();
    expect(frame.display == core::DisplayForm::grid, "the grid moves ran in the grid display");
}

// 8. Ctrl+Home / Ctrl+End は文書の端へ畳む（今の実装のまま・契約で固定）。
void verify_document_edges()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    expect(lands(controller, {at(2, 3), at(2, 8)}, CaretMotion::document_start, at(1, 1)),
           "Ctrl+Home with a selection collapses at the start of the text");
    expect(lands(controller, {at(2, 8), at(2, 3)}, CaretMotion::document_end, at(3, 12)),
           "Ctrl+End with a selection collapses at the end of the text");
}

// 9. Shift つきは今までどおりキャレットから伸ばす。
void verify_shifted_unchanged()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    select(controller, at(2, 6), at(2, 3));
    const auto down =
        controller.apply_frame(move(CaretMotion::next_line, SelectionAnchoring::extend));
    expect(down.caret.position == at(3, 3) && selected(down),
           "Shift+Down extends from the caret, not from the selection's end");
    select(controller, at(1, 5), at(2, 3));
    const auto left =
        controller.apply_frame(move(CaretMotion::previous_character, SelectionAnchoring::extend));
    expect(left.caret.position == at(2, 2) && selected(left),
           "Shift+Left extends one character from the caret");
}

// 10. Vim のモードは変えない（矢印の MoveCaret はキャレットから 1 文字）。
void verify_vim_unchanged()
{
    Editing editing;
    prepare(editing, three_lines, wide);
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::PlaceCaret{at(2, 5), SelectionAnchoring::collapse});
    const auto down = controller.apply_frame(move(CaretMotion::next_line));
    expect(down.caret.position.line == core::LineNumber{3},
           "Vim Down still moves one line from the caret");
}
} // namespace

void verify_selection_arrows_scope()
{
    verify_left_right_collapse();
    verify_up_down_origin();
    verify_home_end_origin();
    verify_page_origin();
    verify_edges_combined();
    verify_wrapped_origin();
    verify_wrapped_collapse_row();
    verify_grid_origin();
    verify_document_edges();
    verify_shifted_unchanged();
    verify_vim_unchanged();
}
} // namespace nenenib::tests
