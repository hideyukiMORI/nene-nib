// ホイールとタッチパッドの端数（#1898・ADR 0009 の決定 6 の 2026-11-20 の追補）。端数の足し引き・
// 行の境の繰り上がり・本文の端・行に揃える意図と保つ意図・split の枠ごと・Vim の鍵の後の 0・
// 端数が 0 の道の行数、と core の画素のずらし（scrolled_body・body_row_top・body_row_at）。
#include "BodyLayout.hpp"
#include "CaretMotion.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "PaneFrame.hpp"
#include "PlaceCaret.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "Scopes.hpp"
#include "ScrollCarry.hpp"
#include "ScrollFraction.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchPane.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <string>
#include <variant>

namespace nenenib::tests
{
namespace
{
using namespace nenenib::core;
using namespace nenenib::application;

// 30 行の文書を 10 行の窓（幅 0・1 行 = 1 画面行）で開き、キャレットを先頭に置く。
void prepare_lines(EditorController &controller)
{
    std::string text;
    for (int line = 1; line <= 30; ++line)
    {
        text += "l" + std::to_string(line) + (line < 30 ? "\n" : "");
    }
    static_cast<void>(controller.apply(InsertText{std::move(text)}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(VisibleLines{10}));
}

[[nodiscard]] bool at(const EditorFrame &frame, std::size_t first, std::size_t fraction)
{
    return frame.first_visible == LineNumber{first} && frame.fraction == fraction;
}

void verify_carry_arithmetic()
{
    constexpr auto down = fine_carried(80, 0, 60);
    static_assert(down.lines == 1 && down.rest == 20, "crossing a row carries one screen row");
    constexpr auto up = fine_carried(20, 0, -60);
    static_assert(up.lines == -1 && up.rest == 80, "the fraction stays non-negative going up");
    constexpr auto whole = fine_carried(40, 3, 0);
    static_assert(whole.lines == 3 && whole.rest == 40, "a whole notch keeps the fraction");
    constexpr auto coarse = coarse_carried(100, 0, 40);
    static_assert(coarse.lines == 1 && coarse.rest == 20, "a coarse target carries the remainder");
    constexpr auto reversed = coarse_carried(100, 0, -40);
    static_assert(reversed.lines == 0 && reversed.rest == -40,
                  "a coarse target drops the remainder when the direction reverses");
    expect(wheel_amount(-2, -30) == -270, "the amount counts lines and parts in one unit");
}

void verify_body_shift()
{
    const auto body = body_layout(800, 600, 96, default_font_size());
    expect(scrolled_body(body, 0).scrolled == 0, "no fraction shifts nothing");
    const auto half = scrolled_body(body, scroll_fraction_units / 2);
    expect(half.scrolled == body.line_height / 2, "half a row shifts half the line height");
    expect(scrolled_body(body, scroll_fraction_units - 1).scrolled < body.line_height,
           "the shift stays under one line height");
    expect(body_row_top(half, 0) == body.content.top - half.scrolled &&
               body_row_top(half, 2) == body.content.top - half.scrolled + (2 * body.line_height),
           "screen rows start above the content top by the shift");
    expect(body_row_at(half, body.content.top) == 0 &&
               body_row_at(half, body.content.top + body.line_height - half.scrolled) == 1 &&
               body_row_at(half, body.content.top - 50) == 0,
           "hit testing reads the same shift as drawing");
    expect(body_row_at(body, body.content.top + body.line_height) == 1,
           "without a fraction a point maps to the row it always did");
}

void verify_small_wheel_accumulates()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_lines(controller);
    expect(controller.frame().lines.size() == 10, "no fraction draws only the visible rows");
    const auto part = controller.apply_frame(ScrollLines{0, 40});
    expect(at(part, 1, 40) && part.lines.size() == 11,
           "a small delta keeps a fraction and draws one more row");
    static_cast<void>(controller.apply(ScrollLines{0, 40}));
    const auto whole = controller.apply_frame(ScrollLines{0, 40});
    expect(at(whole, 2, 0) && whole.lines.size() == 10,
           "three thirds of a row cross the row boundary and settle on it");
    const auto back = controller.apply_frame(ScrollLines{0, -20});
    expect(at(back, 1, 100), "going up across the boundary borrows a row");
    const auto notch = controller.apply_frame(ScrollLines{3});
    expect(at(notch, 4, 100), "a whole notch keeps the fraction");
}

void verify_edges_drop_fraction()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_lines(controller);
    expect(at(controller.apply_frame(ScrollLines{0, -40}), 1, 0),
           "the document start has no fraction");
    static_cast<void>(controller.apply(ScrollLines{100}));
    const auto bottom = controller.frame().first_visible;
    expect(at(controller.apply_frame(ScrollLines{0, 60}), bottom.value, 0),
           "the scroll limit has no fraction");
    const auto below = controller.apply_frame(ScrollLines{0, -60});
    expect(at(below, bottom.value - 1, 60), "leaving the limit upward keeps a fraction");
    expect(at(controller.apply_frame(ScrollLines{0, 90}), bottom.value, 0),
           "reaching the limit again drops the fraction");
}

void verify_keys_align_and_pointer_keeps()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_lines(controller);
    static_cast<void>(controller.apply(ScrollLines{2, 50}));
    const auto clicked = controller.apply_frame(
        PointText{0.0F, 1, SelectionAnchoring::collapse, PointerClicks::single});
    expect(at(clicked, 3, 50), "a click keeps the fraction it hit-tested against");
    const auto typed = controller.apply_frame(InsertText{"x"});
    expect(at(typed, 3, 0), "typing aligns the window to the row without moving its top");
    static_cast<void>(controller.apply(ScrollLines{0, 30}));
    const auto moved =
        controller.apply_frame(MoveCaret{CaretMotion::next_line, SelectionAnchoring::collapse});
    expect(moved.fraction == 0, "a caret move aligns to the row");
}

void verify_vim_keys_align()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_lines(controller);
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    const auto wheel = controller.apply_frame(ScrollLines{1, 60});
    expect(at(wheel, 2, 60), "the wheel keeps a fraction in Vim mode");
    vim_replay(controller, "j");
    expect(controller.frame().fraction == 0, "a Vim key aligns to the row");
}

void verify_tabs_and_panes()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_lines(controller);
    static_cast<void>(controller.apply(SplitPane{SplitLayout::horizontal}));
    const auto other = controller.apply_frame(ScrollPane{0, 50});
    const auto *pane =
        other.other.has_value() ? std::get_if<PaneFrame>(&other.other.value()) : nullptr;
    expect(pane != nullptr && pane->fraction == 50 && other.fraction == 0,
           "each pane keeps its own fraction");
    const auto switched = controller.apply_frame(SwitchPane{});
    expect(switched.fraction == 50, "switching panes carries the pane's fraction");

    Editing tabs;
    auto &tabbed = tabs.controller();
    prepare_lines(tabbed);
    static_cast<void>(tabbed.apply(ScrollLines{0, 70}));
    static_cast<void>(tabbed.apply(NewTab{}));
    const auto restored = tabbed.apply_frame(SwitchTab{0});
    expect(restored.fraction == 0, "restoring a tab starts on a row");
}
} // namespace

void verify_fine_scroll_scope()
{
    verify_carry_arithmetic();
    verify_body_shift();
    verify_small_wheel_accumulates();
    verify_edges_drop_fraction();
    verify_keys_align_and_pointer_keeps();
    verify_vim_keys_align();
    verify_tabs_and_panes();
}
} // namespace nenenib::tests
