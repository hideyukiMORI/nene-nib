// scope `--vim-character-search` の単体テスト（ADR 0042 決定 2）。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterSearch.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimInputWait.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNoEffect.hpp"
#include "VimRegisterKind.hpp"
#include "VimSearchDirection.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
using nenenib::application::OpenDocument;
using nenenib::application::SelectEditMode;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::Selection;
using nenenib::core::TextBuffer;
using nenenib::core::TextPosition;
using nenenib::core::vim_step;
using nenenib::core::VimCharacter;
using nenenib::core::VimCharacterSearchKind;
using nenenib::core::VimColumnWish;
using nenenib::core::VimEditorView;
using nenenib::core::VimInputWait;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimMoveTo;
using nenenib::core::VimNoEffect;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimSelect;
using nenenib::core::VimState;
using nenenib::core::VimViewport;
using nenenib::core::VirtualColumn;

void verify_vim_character_search_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("char-search-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 60, "the scoped character-search suite replays its 60 oracle fixtures");
}

void verify_vim_character_search_history()
{
    const auto text = TextBuffer::from_utf8("axbxc");
    expect(text.has_value(), "the repeated-search sample parses");
    const auto &buffer = text.value();
    const VimState state = empty_vim_state();
    const auto first_waiting = vim_step(
        state, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'f'}});
    const auto first =
        vim_step(first_waiting.next,
                 VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
                 VimKey{VimCharacter{U'x'}});
    const auto repeated = vim_step(
        first.next, VimEditorView{buffer, collapsed_at(Offset{1}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U';'}});
    expect(std::get<VimMoveTo>(repeated.effect).caret == Offset{3} &&
               last_search_is(repeated.next, VimCharacterSearchKind::find_forward, U'x'),
           "; repeats forward without changing the stored search");
    const auto opposite =
        vim_step(repeated.next,
                 VimEditorView{buffer, collapsed_at(Offset{3}), VimViewport{LineNumber{1}, 64}},
                 VimKey{VimCharacter{U','}});
    expect(std::get<VimMoveTo>(opposite.effect).caret == Offset{1} &&
               last_search_is(opposite.next, VimCharacterSearchKind::find_forward, U'x'),
           ", reverses this invocation while preserving the stored direction");
    const auto absent = vim_step(
        state, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U';'}});
    expect(std::holds_alternative<VimNoEffect>(absent.effect),
           "repeat without a stored character search does nothing");
}

void verify_vim_character_search_failure()
{
    const auto text = TextBuffer::from_utf8("abc");
    expect(text.has_value(), "the failed-search sample parses");
    const auto &buffer = text.value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}};
    VimState state = empty_vim_state();
    state.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::till_backward, U'x'};
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto counted = vim_step(state, view, VimKey{VimCharacter{U'3'}});
    const auto pending = vim_step(counted.next, view, VimKey{VimCharacter{U'd'}});
    expect(pending.next.wanted_column == state.wanted_column,
           "starting an operator preserves the line-end column wish");
    const auto waiting = vim_step(pending.next, view, VimKey{VimCharacter{U'f'}});
    const auto failed = vim_step(waiting.next, view, VimKey{VimCharacter{U'z'}});
    expect(std::holds_alternative<VimNoEffect>(failed.effect) &&
               last_search_is(failed.next, VimCharacterSearchKind::find_forward, U'z'),
           "a failed new search still replaces the search history");
    expect(!failed.next.count.has_value() && !failed.next.pending.has_value() &&
               !failed.next.input_wait.has_value() &&
               failed.next.wanted_column == state.wanted_column,
           "a failed operator search clears transients and preserves the wanted column");
}

void verify_vim_character_search_cancellation()
{
    const auto text = TextBuffer::from_utf8("axb");
    expect(text.has_value(), "the cancellation sample parses");
    const auto &buffer = text.value();
    const Selection selection{Offset{0}, Offset{1}};
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, 64}};
    VimState visual = empty_vim_state();
    visual.mode = VimMode::visual;
    visual.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::find_forward, U'x'};
    visual.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto waiting = vim_step(visual, view, VimKey{VimCharacter{U'f'}});
    constexpr std::array cancellations{VimSpecialKey::escape,     VimSpecialKey::backspace,
                                       VimSpecialKey::arrow_left, VimSpecialKey::arrow_right,
                                       VimSpecialKey::arrow_up,   VimSpecialKey::arrow_down,
                                       VimSpecialKey::home,       VimSpecialKey::end,
                                       VimSpecialKey::page_up,    VimSpecialKey::page_down};
    for (const VimSpecialKey key : cancellations)
    {
        const auto cancelled = vim_step(waiting.next, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(cancelled.effect) &&
                   cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value(),
               "a navigation special key cancels only the VISUAL search wait");
        expect(last_search_is(cancelled.next, VimCharacterSearchKind::find_forward, U'x') &&
                   cancelled.next.wanted_column == visual.wanted_column,
               "search cancellation preserves history and the wanted column");
    }
    const auto till_waiting = vim_step(visual, view, VimKey{VimCharacter{U't'}});
    const auto selected = vim_step(till_waiting.next, view, VimKey{VimCharacter{U'b'}});
    expect(std::get<VimSelect>(selected.effect).selection == Selection{Offset{0}, Offset{1}} &&
               selected.next.mode == VimMode::visual,
           "a successful no-move VISUAL search is distinct from cancellation");
}

void verify_vim_character_search_control_targets()
{
    const auto text = TextBuffer::from_utf8("abc\r\ndef");
    expect(text.has_value(), "the special-target CRLF sample parses");
    const auto &buffer = text.value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}};
    constexpr std::array targets{std::pair{VimSpecialKey::enter, char32_t{0x0D}},
                                 std::pair{VimSpecialKey::control_r, char32_t{0x12}},
                                 std::pair{VimSpecialKey::control_d, char32_t{0x04}},
                                 std::pair{VimSpecialKey::control_u, char32_t{0x15}},
                                 std::pair{VimSpecialKey::control_f, char32_t{0x06}},
                                 std::pair{VimSpecialKey::control_b, char32_t{0x02}}};
    for (const auto [key, target] : targets)
    {
        VimState waiting = empty_vim_state();
        waiting.input_wait = VimInputWait{VimCharacterSearchKind::till_forward};
        const auto searched = vim_step(waiting, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(searched.effect) &&
                   last_search_is(searched.next, VimCharacterSearchKind::till_forward, target),
               "a control special key is recorded as its control-code search target");
    }
}

void verify_vim_character_search_large_count()
{
    const auto text = TextBuffer::from_utf8("ax");
    expect(text.has_value(), "the large-count character-search sample parses");
    const auto &buffer = text.value();
    VimState waiting = empty_vim_state();
    waiting.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    waiting.input_wait = VimInputWait{VimCharacterSearchKind::find_forward};
    const auto searched = vim_step(
        waiting, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'x'}});
    expect(std::holds_alternative<VimNoEffect>(searched.effect) &&
               last_search_is(searched.next, VimCharacterSearchKind::find_forward, U'x') &&
               !searched.next.count.has_value(),
           "a huge count scans the bounded line once and reports a miss");
}

void verify_vim_character_search_mode_lifetime()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("a:b;c")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "f:");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}} &&
               !controller.frame().command_line.has_value(),
           "an awaited colon is searched without opening the command line");
    expect(last_search_is(controller.vim_state(), VimCharacterSearchKind::find_forward, U':'),
           "the command character is stored as the ordinary search target");
    vim_replay(controller, "f");
    expect(controller.vim_state().input_wait.has_value(),
           "the engine exposes the pending target state before a mode toggle");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    expect(!controller.vim_state().input_wait.has_value() &&
               last_search_is(controller.vim_state(), VimCharacterSearchKind::find_forward, U':'),
           "mode toggles cancel a wait and preserve completed search history");
}

void verify_vim_character_search_crlf_and_undo()
{
    Editing crlf;
    EditorController &crlf_controller = crlf.controller();
    crlf.files().hold(Bytes{std::string("ax\r\nx")});
    static_cast<void>(crlf_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(crlf_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(crlf_controller, "fx;");
    expect(crlf_controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}},
           "a repeated character search does not cross a CRLF line boundary");

    Editing undo;
    EditorController &undo_controller = undo.controller();
    undo.files().hold(Bytes{std::string("abxc")});
    static_cast<void>(undo_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(undo_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(undo_controller, "dfx");
    expect(vim_body(undo_controller.frame()) == "c",
           "a character-search operator edits through the canonical range effect");
    vim_replay(undo_controller, "u");
    expect(vim_body(undo_controller.frame()) == "abxc",
           "undo restores a character-search operator as one edit");
}

void verify_vim_register_initial_and_empty_yank()
{
    Editing empty;
    EditorController &empty_controller = empty.controller();
    empty.files().hold(Bytes{std::string("abc")});
    static_cast<void>(empty_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(empty_controller.apply(SelectEditMode{EditMode::vim}));
    expect(empty_controller.vim_state().unnamed_register.kind == VimRegisterKind::uninitialized,
           "a new editor starts with an uninitialized unnamed register");
    vim_replay(empty_controller, "pP");
    expect(vim_body(empty_controller.frame()) == "abc" &&
               empty_controller.vim_state().unnamed_register.kind == VimRegisterKind::uninitialized,
           "p and P keep an uninitialized empty register as a no-op");

    Editing yank;
    EditorController &yank_controller = yank.controller();
    yank.files().hold(Bytes{std::string("ax")});
    static_cast<void>(yank_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(yank_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(yank_controller, "yyy0");
    expect(yank_controller.vim_state().unnamed_register.text.empty() &&
               yank_controller.vim_state().unnamed_register.kind == VimRegisterKind::characters,
           "a successful empty characterwise yank clears a seeded register as characterwise");
}

void verify_vim_register_empty_remove_and_change()
{
    for (const std::string_view keys :
         {std::string_view{"yy$dTa"}, std::string_view{"yy$cTa<Esc>"}})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(Bytes{std::string("ax")});
        static_cast<void>(controller.apply(OpenDocument{sample_path()}));
        static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
        vim_replay(controller, keys);
        expect(vim_body(controller.frame()) == "ax" &&
                   controller.vim_state().unnamed_register.text == "ax\n" &&
                   controller.vim_state().unnamed_register.kind == VimRegisterKind::lines,
               "an empty backward-till delete or change preserves a seeded line register");
    }
}

void verify_vim_register_regular_operations()
{
    Editing deleting;
    EditorController &delete_controller = deleting.controller();
    deleting.files().hold(Bytes{std::string("abc")});
    static_cast<void>(delete_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(delete_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(delete_controller, "dl");
    expect(vim_body(delete_controller.frame()) == "bc" &&
               vim_register_kind(delete_controller.vim_state().unnamed_register) == "v",
           "a regular delete still writes a characterwise register");

    Editing changing;
    EditorController &change_controller = changing.controller();
    changing.files().hold(Bytes{std::string("abc")});
    static_cast<void>(change_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(change_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(change_controller, "clx<Esc>");
    expect(vim_body(change_controller.frame()) == "xbc" &&
               vim_register_kind(change_controller.vim_state().unnamed_register) == "v",
           "a regular change still writes a characterwise register");

    for (const std::string_view keys : {std::string_view{"ylp"}, std::string_view{"ylP"}})
    {
        Editing putting;
        EditorController &put_controller = putting.controller();
        putting.files().hold(Bytes{std::string("abc")});
        static_cast<void>(put_controller.apply(OpenDocument{sample_path()}));
        static_cast<void>(put_controller.apply(SelectEditMode{EditMode::vim}));
        vim_replay(put_controller, keys);
        expect(vim_body(put_controller.frame()) == "aabc" &&
                   vim_register_kind(put_controller.vim_state().unnamed_register) == "v",
               "regular yank and characterwise p or P still share the initialized register");
    }
}
} // namespace

void verify_vim_character_search_waiting()
{
    const auto text = TextBuffer::from_utf8("a1ffx");
    expect(text.has_value(), "the character-search sample parses");
    const auto &buffer = text.value();
    const Selection at_start = collapsed_at(Offset{0});
    const VimEditorView view{buffer, at_start, VimViewport{LineNumber{1}, 64}};
    VimState state = empty_vim_state();
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto counted = vim_step(state, view, VimKey{VimCharacter{U'2'}});
    const auto waiting = vim_step(counted.next, view, VimKey{VimCharacter{U'f'}});
    expect(waits_for_character(waiting.next, VimCharacterSearchKind::find_forward) &&
               waiting.next.count == nenenib::core::VimCount{2},
           "f waits for a target without consuming its count");
    const auto found = vim_step(waiting.next, view, VimKey{VimCharacter{U'f'}});
    expect(std::get<VimMoveTo>(found.effect).caret == Offset{3},
           "the awaited f is a target character, not another command");
    expect(last_search_is(found.next, VimCharacterSearchKind::find_forward, U'f') &&
               !found.next.count.has_value() && !found.next.input_wait.has_value(),
           "a successful search records its target and clears transient state");
    expect(found.next.wanted_column ==
               nenenib::core::VimWantedColumn{VimColumnWish::at_column, VirtualColumn{4}},
           "a successful search replaces the line-end column wish with its destination");
    const auto digit_waiting = vim_step(state, view, VimKey{VimCharacter{U'f'}});
    const auto digit = vim_step(digit_waiting.next, view, VimKey{VimCharacter{U'1'}});
    expect(std::get<VimMoveTo>(digit.effect).caret == Offset{1},
           "a digit in target position is consumed as the searched character");
}

void verify_vim_character_search_contracts()
{
    verify_vim_character_search_waiting();
    verify_vim_character_search_history();
    verify_vim_character_search_failure();
    verify_vim_character_search_cancellation();
    verify_vim_character_search_control_targets();
    verify_vim_character_search_large_count();
    verify_vim_character_search_mode_lifetime();
    verify_vim_character_search_crlf_and_undo();
    verify_vim_register_initial_and_empty_yank();
    verify_vim_register_empty_remove_and_change();
    verify_vim_register_regular_operations();
}

void verify_vim_character_search_scope()
{
    verify_vim_character_search_contracts();
    verify_vim_character_search_fixtures();
    verify_vim_step_edges();
    verify_vim_visual_step_edges();
    verify_vim_viewport_mode_edges();
    verify_vim_insert_undo_unit();
    verify_vim_change_undo_unit();
    verify_vim_put_line_endings();
    verify_vim_crlf();
}
} // namespace nenenib::tests
