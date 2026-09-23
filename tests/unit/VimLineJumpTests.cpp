// scope `--vim-line-jumps` の単体テスト（ADR 0042 決定 2）。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterSearch.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNoEffect.hpp"
#include "VimOperator.hpp"
#include "VimPendingOperator.hpp"
#include "VimPrefix.hpp"
#include "VimRegisterKind.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
using nenenib::application::OpenDocument;
using nenenib::application::SaveDocument;
using nenenib::application::SelectEditMode;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Selection;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::toggled;
using nenenib::core::vim_step;
using nenenib::core::VimCharacter;
using nenenib::core::VimCharacterSearchKind;
using nenenib::core::VimColumnWish;
using nenenib::core::VimEditorView;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimMoveTo;
using nenenib::core::VimNoEffect;
using nenenib::core::VimPrefix;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimState;
using nenenib::core::VimViewport;
using nenenib::core::VirtualColumn;

void verify_vim_line_jump_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("line-jump-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 41, "the scoped line-jump suite replays its 41 oracle fixtures");
}

void verify_vim_line_jump_text_edges()
{
    Editing empty;
    EditorController &empty_controller = empty.controller();
    static_cast<void>(empty_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(empty_controller, "Ggg");
    expect(empty_controller.frame().caret.position == TextPosition{LineNumber{1}, Column{1}},
           "G and gg stay at the only position in an empty document");

    Editing trailing;
    EditorController &trailing_controller = trailing.controller();
    trailing.files().hold(Bytes{std::string("one\r\n")});
    static_cast<void>(trailing_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(trailing_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(trailing_controller, "G");
    expect(trailing_controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "G reaches the trailing empty CRLF line");
    static_cast<void>(trailing_controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(trailing.files().written() == "one\r\n",
           "a line jump preserves the trailing CRLF bytes");
}

void verify_vim_line_jump_saturated_count()
{
    const auto text = TextBuffer::from_utf8("one\ntwo\nthree");
    expect(text.has_value(), "the saturated line-count sample parses");
    const auto &buffer = text.value();
    VimState state = empty_vim_state();
    state.pending = nenenib::core::VimPendingOperator{
        nenenib::core::VimOperator::yank,
        nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()}};
    state.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    const VimEditorView view{buffer, collapsed_at(buffer.line_start(LineNumber{2})),
                             VimViewport{LineNumber{1}, 64}};
    const auto waiting = vim_step(state, view, VimKey{VimCharacter{U'g'}});
    const auto jumped = vim_step(waiting.next, view, VimKey{VimCharacter{U'g'}});
    expect(std::get<VimMoveTo>(jumped.effect).caret == buffer.line_start(LineNumber{2}) &&
               jumped.next.unnamed_register.text == "two\nthree\n" &&
               jumped.next.unnamed_register.kind == VimRegisterKind::lines,
           "a saturated operator-motion count clamps to the final document line");
}
} // namespace

void verify_vim_line_jump_waiting()
{
    const auto text = TextBuffer::from_utf8("one\ntwo\nthree");
    expect(text.has_value(), "the prefix-wait sample parses");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{2}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, 64}};
    VimState state = empty_vim_state();
    state.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::find_forward, U'x'};
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};

    const auto operation_count = vim_step(state, view, VimKey{VimCharacter{U'2'}});
    const auto pending = vim_step(operation_count.next, view, VimKey{VimCharacter{U'd'}});
    const auto motion_count = vim_step(pending.next, view, VimKey{VimCharacter{U'3'}});
    const auto waiting = vim_step(motion_count.next, view, VimKey{VimCharacter{U'g'}});
    expect(waits_for_prefix(waiting.next, VimPrefix::g) && waiting.next.count.has_value() &&
               waiting.next.count.value().value == 3 && waiting.next.pending.has_value() &&
               waiting.next.pending.value().count.has_value() &&
               waiting.next.pending.value().count.value().value == 2,
           "g waits without collapsing the operator and motion counts");
    const auto invalid = vim_step(waiting.next, view, VimKey{VimCharacter{U'3'}});
    expect(std::holds_alternative<VimNoEffect>(invalid.effect) &&
               !invalid.next.input_wait.has_value() && !invalid.next.count.has_value() &&
               !invalid.next.pending.has_value(),
           "an unsupported g suffix is consumed and clears transient input state");
    expect(invalid.next.mode == state.mode && invalid.next.wanted_column == state.wanted_column &&
               last_search_is(invalid.next, VimCharacterSearchKind::find_forward, U'x'),
           "an unsupported g suffix preserves mode, wanted column, and search memory");

    VimState visual = state;
    visual.mode = VimMode::visual;
    const auto visual_waiting = vim_step(visual, view, VimKey{VimCharacter{U'g'}});
    for (const VimSpecialKey key : {VimSpecialKey::escape, VimSpecialKey::page_down})
    {
        const auto cancelled = vim_step(visual_waiting.next, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(cancelled.effect) &&
                   cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value(),
               "a special key cancels only the VISUAL g wait");
        expect(cancelled.next.wanted_column == visual.wanted_column &&
                   last_search_is(cancelled.next, VimCharacterSearchKind::find_forward, U'x'),
               "g cancellation preserves the wanted column and search memory");
    }

    const auto search_waiting = vim_step(state, view, VimKey{VimCharacter{U'f'}});
    const auto searched = vim_step(search_waiting.next, view, VimKey{VimCharacter{U'g'}});
    expect(last_search_is(searched.next, VimCharacterSearchKind::find_forward, U'g'),
           "g is a literal target while a character search is waiting");
}

void verify_vim_line_jump_continuations()
{
    for (const std::string_view keys : {"g3G", "gZG", "gGG"})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(Bytes{std::string("one\ntwo\n  three")});
        static_cast<void>(controller.apply(OpenDocument{sample_path()}));
        static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
        vim_replay(controller, keys);
        expect(controller.frame().caret.position == TextPosition{LineNumber{3}, Column{3}},
               "a separately delivered G works after an unsupported g suffix");
    }

    Editing visual;
    EditorController &visual_controller = visual.controller();
    visual.files().hold(Bytes{std::string("abcd\nx\nabcdef")});
    static_cast<void>(visual_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(visual_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(visual_controller, "v$g<Esc>jj");
    const TextPosition visual_position = visual_controller.frame().caret.position;
    expect(visual_controller.vim_state().mode == VimMode::visual &&
               visual_position == TextPosition{LineNumber{3}, Column{7}},
           "a cancelled VISUAL g wait keeps the line-end wish for later vertical moves");

    Editing toggled;
    EditorController &toggle_controller = toggled.controller();
    static_cast<void>(toggle_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(toggle_controller, "g");
    expect(waits_for_prefix(toggle_controller.vim_state(), VimPrefix::g),
           "the controller exposes a pending g prefix");
    static_cast<void>(toggle_controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(toggle_controller.apply(SelectEditMode{EditMode::vim}));
    expect(!toggle_controller.vim_state().input_wait.has_value(),
           "a mode toggle cancels a pending g prefix");
}

void verify_vim_line_jump_operator_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\r\ntwo\r\nthree")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "2Gdgg");
    expect(vim_body(controller.frame()) == "three",
           "dgg edits through the shared linewise operator path");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "one\ntwo\nthree",
           "one undo restores the complete line-jump edit");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "one\r\ntwo\r\nthree",
           "undo restores the original CRLF bytes");
}

void verify_vim_line_jump_contracts()
{
    verify_vim_line_jump_waiting();
    verify_vim_line_jump_text_edges();
    verify_vim_line_jump_continuations();
    verify_vim_line_jump_saturated_count();
    verify_vim_line_jump_operator_undo();
}

void verify_vim_line_jump_scope()
{
    verify_vim_line_jump_contracts();
    verify_vim_line_jump_fixtures();
    verify_vim_character_search_scope();
}
} // namespace nenenib::tests
