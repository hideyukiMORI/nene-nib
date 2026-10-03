// Issue #282 / ADR 0065。通常の文字と共有している Vim INSERT の境界。
#include "CaretMove.hpp"
#include "DeleteText.hpp"
#include "Editing.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "OpenDocument.hpp"
#include "OrdinaryCharacterBoundary.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using Action = core::OrdinaryCharacterAction;
using core::Offset;

Offset boundary(std::string_view input, std::size_t at, Action action)
{
    return core::ordinary_character_boundary(buffer_of(input), Offset{at}, action);
}

void verify_grouped_characters()
{
    for (const std::string_view input :
         {"e\u0301", "e\u0301\u0323", "か\u3099", "は\u309A", "葛\U000E0100",
          "\U00020000\U000E01EF", "\u2764\uFE0F", "a\uFE00", "\u0301\u0323"})
    {
        expect(boundary(input, 0, Action::next) == Offset{input.size()},
               "right crosses a base with its combining marks or selector");
        expect(boundary(input, input.size(), Action::previous) == Offset{0},
               "left crosses the same ordinary character");
        expect(boundary(input, 0, Action::erase_forward) == Offset{input.size()},
               "Delete removes the whole ordinary character");
        expect(boundary(input, input.size(), Action::containing) == Offset{input.size()},
               "the end of the line remains a valid resting caret");
    }
}

void verify_backspace()
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 10> examples{{
        {"e\u0301", "e"},
        {"e\u0301\u0323", "e\u0301"},
        {"か\u3099", "か"},
        {"葛\U000E0100", ""},
        {"\U00020000\U000E01EF", ""},
        {"\u2764\uFE0F", ""},
        {"e\u0301\uFE0F", "e"},
        {"e\u0301\u0323\uFE0F", "e\u0301"},
        {"e\uFE0F\uFE0E", ""},
        {"\u0301\u0323", "\u0301"},
    }};
    for (const auto &[input, remaining] : examples)
    {
        expect(boundary(input, input.size(), Action::backspace) == Offset{remaining.size()},
               "Backspace removes one mark or a selector sequence and its preceding point");
    }
}

void verify_selector_edges()
{
    constexpr std::string_view after = "e\uFE0F\u0301";
    expect(boundary(after, 0, Action::next) == Offset{4}, "a selector ends the first group");
    expect(boundary(after, 4, Action::next) == Offset{6}, "a mark after a selector is separate");
    expect(boundary(after, 6, Action::previous) == Offset{4}, "left preserves that boundary");
    expect(boundary(after, 4, Action::containing) == Offset{4}, "that mark is a valid start");
    expect(boundary(after, 0, Action::erase_forward) == Offset{4}, "Delete leaves that mark");
    expect(boundary("e\uFE0F\uFE0E", 0, Action::next) == Offset{4},
           "movement stops after the first selector");
    expect(boundary("e\uFE0F\uFE0E", 0, Action::erase_forward) == Offset{7},
           "Delete consumes consecutive selectors");
    expect(boundary("\uFE0F", 3, Action::backspace) == Offset{0}, "an orphan selector terminates");
    expect(boundary("\n\uFE0F", 4, Action::backspace) == Offset{1},
           "an orphan selector cannot pull in a newline");
    expect(boundary("\n\uFE0F", 0, Action::erase_forward) == Offset{1},
           "deleting a newline does not consume the next line's selector");
}

void verify_plain_and_line_boundaries()
{
    for (const auto action : {Action::previous, Action::next, Action::containing, Action::backspace,
                              Action::erase_forward})
    {
        expect(boundary("", 0, action) == Offset{0}, "empty text is stable for every action");
    }
    expect(boundary("a日\U0001F600", 1, Action::next) == Offset{4}, "CJK UTF-8 stays whole");
    expect(boundary("a日\U0001F600", 4, Action::next) == Offset{8}, "a surrogate pair stays whole");
    expect(boundary("a日\U0001F600", 8, Action::backspace) == Offset{4},
           "Backspace keeps UTF-8 valid");
    for (const std::string_view input : {"a\r\n\u0301", "a\n\u0301"})
    {
        const std::size_t next_line = input.size() - 2;
        expect(boundary(input, 1, Action::next) == Offset{next_line}, "right crosses one newline");
        expect(boundary(input, next_line, Action::previous) == Offset{1},
               "left crosses one newline");
        expect(boundary(input, 1, Action::erase_forward) == Offset{next_line},
               "Delete crosses one newline");
        expect(boundary(input, next_line, Action::backspace) == Offset{1},
               "Backspace crosses one newline");
        expect(boundary(input, next_line, Action::containing) == Offset{next_line},
               "marks stay on their line");
    }
    expect(boundary("a\t\u0301", 1, Action::next) == Offset{2}, "a tab is its own unit");
}

void verify_landing_and_long_runs()
{
    const auto text = buffer_of("ab\ne\u0301x\nab");
    for (const auto motion : {core::CaretMotion::next_line, core::CaretMotion::page_down})
    {
        expect(core::moved_caret(text, Offset{1},
                                 core::CaretMoveRequest{motion, 1, core::CaretUnit::ordinary}) ==
                   Offset{3},
               "vertical landing snaps a combining mark to its base");
        expect(core::moved_caret(text, Offset{1},
                                 core::CaretMoveRequest{motion, 1, core::CaretUnit::code_point}) ==
                   Offset{4},
               "the Vim code-point movement contract is unchanged");
    }
    for (const auto motion : {core::CaretMotion::previous_line, core::CaretMotion::page_up})
    {
        expect(core::moved_caret(text, Offset{9},
                                 core::CaretMoveRequest{motion, 1, core::CaretUnit::ordinary}) ==
                   Offset{3},
               "upward landing also snaps to the base");
    }
    std::string input(200000, 'x');
    const std::size_t start = input.size();
    input += "e";
    for (std::size_t index = 0; index < 300; ++index)
    {
        input += "\u0301";
    }
    auto fragmented = buffer_of(input).insert(Offset{start + 1}, "\u0323");
    expect(core::ordinary_character_boundary(fragmented, Offset{start}, Action::next) ==
               Offset{fragmented.size_bytes()},
           "a long mark run crosses piece boundaries without a fixed lookaround limit");
    expect(core::ordinary_character_boundary(fragmented, Offset{fragmented.size_bytes()},
                                             Action::previous) == Offset{start},
           "left finds the base of the same long fragmented run");
}

void open_ordinary(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::ordinary});
}

void verify_controller_deletion()
{
    for (const auto direction : {core::DeleteDirection::forward, core::DeleteDirection::backward})
    {
        Editing editing;
        auto &controller = editing.controller();
        open_ordinary(editing, "e\u0301");
        if (direction == core::DeleteDirection::backward)
        {
            applied(controller, app::MoveCaret{core::CaretMotion::document_end,
                                               core::SelectionAnchoring::collapse});
        }
        const std::string expected = direction == core::DeleteDirection::forward ? "" : "e";
        expect(applied(controller, app::DeleteText{direction}) == expected,
               "controller uses the direction's ordinary deletion unit");
        expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}) == "e\u0301",
               "undo restores the whole deletion once");
        expect(applied(controller, app::HistoryAction{core::HistoryDirection::redo}) == expected,
               "redo repeats the same deletion");
    }
    Editing editing;
    auto &controller = editing.controller();
    open_ordinary(editing, "葛\U000E0100x");
    applied(controller,
            app::MoveCaret{core::CaretMotion::next_character, core::SelectionAnchoring::extend});
    expect(controller.frame().caret.position.column == core::Column{3},
           "Shift+Right extends across the selector");
    expect(applied(controller, app::DeleteText{core::DeleteDirection::backward}) == "x",
           "a selected group is deleted as the exact selection");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}) == "葛\U000E0100x",
           "undo restores the selected group");
}

void verify_vim_insert_boundary()
{
    Editing editing;
    auto &controller = editing.controller();
    open_ordinary(editing, "e\u0301x");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_replay(controller, "A<Left><Left>");
    expect(controller.frame().caret.position.column == core::Column{2},
           "Vim INSERT left still moves one code point");
    vim_replay(controller, "<Right>");
    expect(controller.frame().caret.position.column == core::Column{3},
           "Vim INSERT right still moves one code point");
    expect(vim_body(controller.frame()) == "e\u0301x",
           "Vim insert movement does not edit the text");
    vim_replay(controller, "<Esc>0");
    expect(applied(controller, app::DeleteText{core::DeleteDirection::forward}) == "\u0301x",
           "an external Vim DeleteText keeps its existing code-point meaning");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}) == "e\u0301x",
           "that external deletion still uses the common undo path");
}
} // namespace

void verify_ordinary_character_contracts()
{
    verify_grouped_characters();
    verify_backspace();
    verify_selector_edges();
    verify_plain_and_line_boundaries();
    verify_landing_and_long_runs();
    verify_controller_deletion();
    verify_vim_insert_boundary();
}

void verify_ordinary_character_scope()
{
    verify_ordinary_character_contracts();
    verify_caret_movement_contracts();
    verify_vim_open_line_movement();
}
} // namespace nenenib::tests
