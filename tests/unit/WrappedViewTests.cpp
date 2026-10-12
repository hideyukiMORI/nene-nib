#include "BodyLayout.hpp"
#include "CancelComposition.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "DeleteText.hpp"
#include "Editing.hpp"
#include "ExResult.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "PlaceCaret.hpp"
#include "PointText.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VisibleLines.hpp"

#include <limits>
#include <string>

namespace nenenib::tests
{
namespace
{
using namespace nenenib::core;
using namespace nenenib::application;

void prepare_view(EditorController &controller, std::string text, std::size_t height = 3)
{
    static_cast<void>(controller.apply(InsertText{std::move(text)}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(VisibleLines{height, TextViewGeometry{40, 20, 96}}));
}

void verify_wrapped_rows()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghij\nkl\nmnop");
    const auto initial = controller.frame();
    expect(initial.lines.size() == 1 && initial.lines.front().screen_rows == 3 &&
               initial.lines.front().screen_row == 0 && initial.lines.front().text == "abcdefghij",
           "wrapping fills the viewport without inserting line breaks");
    const auto scrolled = controller.apply_frame(ScrollLines{1});
    expect(scrolled.lines.size() == 2 && scrolled.lines.front().screen_row == -1 &&
               scrolled.lines.at(1).screen_row == 2,
           "one screen-row scroll keeps the partly visible logical line");
    const auto end = controller.apply_frame(ScrollLines{std::numeric_limits<std::int32_t>::max()});
    expect(end.lines.front().number == LineNumber{1} && end.lines.front().screen_row == -2,
           "ordinary scrolling stops at a filled final viewport");
    const auto start =
        controller.apply_frame(ScrollLines{std::numeric_limits<std::int32_t>::min()});
    expect(start.lines.front().number == LineNumber{1} && start.lines.front().screen_row == 0,
           "the minimum signed scroll amount clamps to the document start");
}

void verify_padded_body_view()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghij");
    const auto body = body_layout(110, 180, 96, default_font_size());
    static_cast<void>(controller.apply(VisibleLines{
        body.visible_lines, TextViewGeometry{width_of(body.content), body.line_height, 96}}));
    const auto wrapped = controller.frame();
    expect(wrapped.lines.front().screen_rows == 3 && wrapped.lines.front().text == "abcdefghij",
           "the padded body width wraps the same text into three visual rows");
    const auto margin =
        controller.apply_frame(PointText{static_cast<float>(body.gutter.right - body.content.left),
                                         0, SelectionAnchoring::collapse});
    expect(margin.caret.position.column == Column{1},
           "clicking the padding lands at the start of the visual row");
    const auto second_row = controller.apply_frame(PointText{0, 1, SelectionAnchoring::collapse});
    expect(second_row.caret.position.column == Column{5},
           "the second visual row starts after the four characters that fit the padded width");
    const auto selected = controller.apply_frame(PointText{10, 0, SelectionAnchoring::extend});
    expect(selected.caret.position.column == Column{2} &&
               selected.lines.front().selection.begin == Column{2} &&
               selected.lines.front().selection.end == Column{5},
           "dragging back across the padded wrap boundary preserves the anchor");
}

void verify_wrapped_motion()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghij\nxyz");
    auto frame =
        controller.apply_frame(MoveCaret{CaretMotion::line_end, SelectionAnchoring::collapse});
    expect(frame.caret.position.column == Column{5} &&
               frame.caret.affinity == CaretAffinity::trailing,
           "End stays on the right side of the first wrapped row");
    frame =
        controller.apply_frame(MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    expect(frame.caret.position.column == Column{1},
           "Home uses the same affinity at a wrap boundary");
    static_cast<void>(controller.apply(PointText{20, 0, SelectionAnchoring::collapse}));
    frame = controller.apply_frame(MoveCaret{CaretMotion::next_line, SelectionAnchoring::collapse});
    expect(frame.caret.position.column == Column{7}, "Down moves one visual row at the desired x");
    frame = controller.apply_frame(MoveCaret{CaretMotion::next_line, SelectionAnchoring::collapse});
    expect(frame.caret.position.column == Column{11}, "Down clamps to a short final visual row");
    frame = controller.apply_frame(MoveCaret{CaretMotion::next_line, SelectionAnchoring::collapse});
    expect(frame.caret.position == TextPosition{LineNumber{2}, Column{3}},
           "Down preserves desired x when crossing a logical line");
    frame =
        controller.apply_frame(MoveCaret{CaretMotion::previous_line, SelectionAnchoring::collapse});
    expect(frame.caret.position == TextPosition{LineNumber{1}, Column{11}},
           "Up re-enters the last screen row of the previous logical line");
}

void verify_wrapped_selection()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghij\nklm", 4);
    static_cast<void>(controller.apply(PointText{20, 2, SelectionAnchoring::collapse}));
    const auto reverse = controller.apply_frame(PointText{10, 0, SelectionAnchoring::extend});
    expect(reverse.caret.position.column == Column{2} &&
               reverse.lines.front().selection.begin == Column{2} &&
               reverse.lines.front().selection.end == Column{11},
           "reverse drag retains the original anchor across wrapped rows");
    const auto erased = controller.apply_frame(DeleteText{DeleteDirection::backward});
    expect(erased.lines.front().text == "a", "Backspace removes exactly the dragged source range");
    const auto restored = controller.apply_frame(HistoryAction{HistoryDirection::undo});
    expect(restored.lines.front().text == "abcdefghij", "undo restores the selected logical text");
    static_cast<void>(controller.apply(PointText{10, 0, SelectionAnchoring::collapse}));
    const auto across = controller.apply_frame(PointText{20, 3, SelectionAnchoring::extend});
    expect(across.caret.position == TextPosition{LineNumber{2}, Column{3}} &&
               across.lines.front().selection.end.value > 11,
           "dragging across logical lines includes their newline");
}

void verify_wrapped_composition()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcd", 2);
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{4}}, SelectionAnchoring::collapse}));
    const auto composing = controller.apply_frame(ComposeText{composed_of("日本語", {}, 9)});
    const auto &inserted = composing.lines.front().composed;
    expect(composing.lines.front().text == "abcd" && composing.lines.front().screen_rows == 2 &&
               inserted.has_value() && inserted.value().text == "abc日本語d" &&
               inserted.value().cursor == Column{7},
           "composition owns a shared wrapped display while leaving source text untouched");
    const auto clicked = controller.apply_frame(PointText{0, 0, SelectionAnchoring::collapse});
    expect(clicked.caret.position.column == Column{4},
           "mouse input does not move an active composition");
    const auto cancelled = controller.apply_frame(CancelComposition{});
    expect(!cancelled.lines.front().composed.has_value() &&
               cancelled.lines.front().screen_rows == 1,
           "cancel removes the inserted display and restores row geometry");
    static_cast<void>(controller.apply(ComposeText{composed_of("日本語", {}, 9)}));
    const auto committed = controller.apply_frame(CommitText{"日本語"});
    expect(committed.lines.front().text == "abc日本語d",
           "IME commit inserts the whole string once");
    const auto undone = controller.apply_frame(HistoryAction{HistoryDirection::undo});
    expect(undone.lines.front().text == "abcd", "one undo removes the full IME commit");
}

void verify_wrapped_tabs_and_resize()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghijklmnop", 2);
    static_cast<void>(controller.apply(ScrollLines{1}));
    static_cast<void>(controller.apply(NewTab{}));
    const auto restored = controller.apply_frame(SwitchTab{0});
    expect(restored.lines.front().screen_row == -1,
           "tab switching retains the within-line scroll row");
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{13}}, SelectionAnchoring::collapse}));
    const auto wider = controller.apply_frame(VisibleLines{2, TextViewGeometry{80, 20, 96}});
    expect(wider.lines.front().screen_row == 0 && wider.lines.front().screen_rows == 2 &&
               wider.caret.position.column == Column{13},
           "resize recomputes screen rows and keeps the same source caret visible");
}

void verify_wrapped_failure_and_bounds()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, std::string(10000, '\n'), 3);
    const auto before = editing.text_layouts().requests();
    const auto moved =
        controller.apply_frame(MoveCaret{CaretMotion::next_line, SelectionAnchoring::collapse});
    expect(!moved.layout_failure.has_value() && editing.text_layouts().requests() - before < 40,
           "one screen-row movement shapes bounded nearby lines, not the complete document");
    editing.text_layouts().reject();
    const auto failed = controller.apply(PointText{0, 1, SelectionAnchoring::collapse});
    expect(failed.layout_failure.has_value(),
           "hit-test failure reaches delivery as a typed failure");
    expect(controller.frame().layout_failure.has_value(),
           "frame construction preserves layout failure");
}

void verify_wrap_settings()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghijklmnop", 2);
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{13}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(OpenCommandPalette{}));
    static_cast<void>(controller.apply(CommandText{":set nowrap"}));
    const auto unwrapped = controller.apply_frame(SubmitCommand{});
    expect(unwrapped.settings.wrapping == LineWrapping::no_wrap &&
               unwrapped.lines.front().screen_rows == 1 && unwrapped.horizontal > 0 &&
               unwrapped.caret.position.column == Column{13},
           "ordinary command palette disables wrapping and follows the caret horizontally");
    static_cast<void>(controller.apply(OpenCommandPalette{}));
    static_cast<void>(controller.apply(CommandText{":set wrap"}));
    const auto wrapped = controller.apply_frame(SubmitCommand{});
    expect(wrapped.settings.wrapping == LineWrapping::wrap && wrapped.horizontal == 0 &&
               wrapped.lines.front().screen_rows == 4,
           "wrapping restores visual rows without changing source text");
    expect(editing.settings().writes() == 2, "wrap changes use the existing settings persistence");
    expect(command_completions("set now") == std::vector<std::string>{"set nowrap"},
           "nowrap is exposed through the shared completion catalog");
    editing.settings().fail(SettingsFailure::unwritable);
    static_cast<void>(controller.apply(OpenCommandPalette{}));
    static_cast<void>(controller.apply(CommandText{":set nowrap"}));
    const auto failed = controller.apply_frame(SubmitCommand{});
    expect(failed.settings.wrapping == LineWrapping::wrap && failed.settings_failure.has_value(),
           "a failed settings write leaves wrapping enabled");
}

void verify_unwrapped_drag()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_view(controller, "abcdefghijklmnop", 2);
    static_cast<void>(controller.apply(OpenCommandPalette{}));
    static_cast<void>(controller.apply(CommandText{":set nowrap"}));
    static_cast<void>(controller.apply(SubmitCommand{}));
    static_cast<void>(controller.apply(PointText{10, 0, SelectionAnchoring::collapse}));
    const auto right = controller.apply_frame(PointText{60, 0, SelectionAnchoring::extend});
    expect(right.horizontal > 0 && right.caret.position.column == Column{7},
           "dragging past the right edge follows the caret horizontally");
    const auto left = controller.apply_frame(PointText{-60, 0, SelectionAnchoring::extend});
    expect(left.horizontal == 0 && left.caret.position.column == Column{1} &&
               left.lines.front().selection.end == Column{2},
           "dragging past the left edge returns to the start while retaining the anchor");
}
} // namespace

void verify_wrapped_view_scope()
{
    verify_wrapped_rows();
    verify_padded_body_view();
    verify_wrapped_motion();
    verify_wrapped_selection();
    verify_wrapped_composition();
    verify_wrapped_tabs_and_resize();
    verify_wrapped_failure_and_bounds();
    verify_wrap_settings();
    verify_unwrapped_drag();
}
} // namespace nenenib::tests
