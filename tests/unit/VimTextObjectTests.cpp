// scope `--vim-text-objects` の単体テスト（ADR 0042 決定 2）。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionPresence.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterSearch.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimNoEffect.hpp"
#include "VimOperator.hpp"
#include "VimPendingOperator.hpp"
#include "VimRegisterKind.hpp"
#include "VimRemoveRange.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimTextObjectScope.hpp"
#include "VimViewport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
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
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::Selection;
using nenenib::core::SelectionPresence;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::vim_step;
using nenenib::core::VimCharacter;
using nenenib::core::VimCharacterSearchKind;
using nenenib::core::VimColumnWish;
using nenenib::core::VimEditorView;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimNoEffect;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimState;
using nenenib::core::VimViewport;
using nenenib::core::VirtualColumn;

// ---------------------------------------------------------------- テキストオブジェクト（ADR 0031）

[[nodiscard]] bool waits_for_object(const VimState &state,
                                    nenenib::core::VimTextObjectScope scope) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<nenenib::core::VimTextObjectScope>(state.input_wait.value()) &&
           std::get<nenenib::core::VimTextObjectScope>(state.input_wait.value()) == scope;
}

void verify_vim_text_object_waiting()
{
    const auto buffer = TextBuffer::from_utf8("alpha beta gamma").value();
    const VimEditorView view{buffer, collapsed_at(Offset{6}), VimViewport{LineNumber{1}, 8}};
    VimState initial = empty_vim_state();
    initial.unnamed_register = {"saved", VimRegisterKind::characters};
    const auto pending = vim_step(vim_step(initial, view, VimKey{VimCharacter{U'2'}}).next, view,
                                  VimKey{VimCharacter{U'd'}});
    const auto inner = vim_step(pending.next, view, VimKey{VimCharacter{U'i'}});
    const auto kept = inner.next.pending.has_value() ? inner.next.pending.value().count
                                                     : std::optional<nenenib::core::VimCount>{};
    expect(waits_for_object(inner.next, nenenib::core::VimTextObjectScope::inner) &&
               kept == nenenib::core::VimCount{2} &&
               std::holds_alternative<nenenib::core::VimNoEffect>(inner.effect),
           "an operator's i waits for one object key and keeps the operator count");
    const auto around = vim_step(pending.next, view, VimKey{VimCharacter{U'a'}});
    expect(waits_for_object(around.next, nenenib::core::VimTextObjectScope::around),
           "an operator's a waits for the same key with the around scope");
    const auto inserted = vim_step(initial, view, VimKey{VimCharacter{U'i'}});
    expect(inserted.next.mode == VimMode::insert && !inserted.next.input_wait.has_value(),
           "i without a pending operator is still the insert command");
    const auto completed = vim_step(inner.next, view, VimKey{VimCharacter{U'w'}});
    expect(std::get<nenenib::core::VimRemoveRange>(completed.effect).range ==
               OffsetRange{Offset{6}, Offset{11}},
           "d2iw covers the word and the blanks after it");
    expect(completed.next.mode == VimMode::normal && !completed.next.input_wait.has_value() &&
               !completed.next.pending.has_value() && !completed.next.count.has_value(),
           "a finished object leaves no wait, operator or count behind");
}

void verify_vim_text_object_cancellation()
{
    const auto buffer = TextBuffer::from_utf8("alpha beta gamma").value();
    const Selection selected{Offset{2}, Offset{4}};
    const VimEditorView view{buffer, selected, VimViewport{LineNumber{1}, 8}};
    VimState initial = empty_vim_state();
    initial.mode = VimMode::visual;
    initial.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    initial.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::find_forward, U'b'};
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'i'}});
    expect(waits_for_object(waiting.next, nenenib::core::VimTextObjectScope::inner) &&
               waiting.next.mode == VimMode::visual,
           "VISUAL i is the object prefix, not an insert command");
    for (const VimKey &key : {VimKey{VimCharacter{U'x'}}, VimKey{VimCharacter{U'3'}},
                              VimKey{VimSpecialKey::escape}, VimKey{VimSpecialKey::arrow_left}})
    {
        const auto cancelled = vim_step(waiting.next, view, key);
        expect(cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value() &&
                   cancelled.next.wanted_column == initial.wanted_column &&
                   last_search_is(cancelled.next, VimCharacterSearchKind::find_forward, U'b') &&
                   std::holds_alternative<nenenib::core::VimNoEffect>(cancelled.effect),
               "an unknown object key or Esc keeps the mode, selection, wish and search memory");
    }
    VimState operating = empty_vim_state();
    operating.pending =
        nenenib::core::VimPendingOperator{nenenib::core::VimOperator::remove, std::nullopt};
    const auto pending_wait = vim_step(operating, view, VimKey{VimCharacter{U'a'}});
    const auto dropped = vim_step(pending_wait.next, view, VimKey{VimSpecialKey::escape});
    expect(dropped.next.mode == VimMode::normal && !dropped.next.pending.has_value() &&
               !dropped.next.input_wait.has_value(),
           "cancelling an object drops the pending operator without touching the text");
}

void verify_vim_text_object_visual()
{
    Editing editing;
    open_vim_document(editing, "start(\n  body one\n  body two\n) end");
    EditorController &controller = editing.controller();
    vim_replay(controller, "jvi(");
    expect(controller.vim_state().mode == VimMode::visual,
           "a linewise inner block keeps VISUAL characterwise (measured against Vim 9.1)");
    vim_replay(controller, "y");
    expect(controller.vim_state().unnamed_register.text == "  body one\n  body two\n" &&
               controller.vim_state().unnamed_register.kind == VimRegisterKind::characters,
           "the VISUAL selection reaches through the last line break");
    vim_replay(controller, "Vi(");
    expect(controller.vim_state().mode == VimMode::visual,
           "an object switches linewise VISUAL to characterwise");
    vim_replay(controller, "<Esc>");
    Editing words;
    open_vim_document(words, "alpha beta gamma");
    EditorController &other = words.controller();
    vim_replay(other, "2lviw");
    expect(other.frame().lines.at(0).selection.presence != SelectionPresence::absent,
           "viw selects the word under the caret");
    vim_replay(other, "iwy");
    expect(other.vim_state().unnamed_register.text == "alpha ",
           "a second iw grows the selection by one more chunk");
    vim_replay(other, "2lvi(y");
    expect(other.vim_state().unnamed_register.text == "p",
           "an object that is not found leaves the one-character selection alone");
}

void verify_vim_text_object_history()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\nalpha beta\r\ngh";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "jciwZZ<Esc>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nZZ beta\r\ngh",
           "ciw changes the word and preserves the document CRLF bytes");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores a change through an object");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nZZ beta\r\ngh", "redo restores the change");
    vim_replay(controller, "u");
    vim_replay(controller, "daw");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nbeta\r\ngh", "daw takes the word and its blanks");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores a delete through an object");
}

void verify_vim_text_object_dot()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma delta");
    EditorController &controller = editing.controller();
    vim_replay(controller, "diw");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "diw"),
           "an object command records its own keys with no extra machinery");
    vim_replay(controller, "w.");
    expect(vim_body(controller.frame()) == "  gamma delta",
           ". replays the object command at the new caret");
    Editing quoted;
    open_vim_document(quoted, "a \"bb\" c \"dd\" e");
    EditorController &other = quoted.controller();
    vim_replay(other, "3lci\"x<Esc>");
    expect(dot_record_is(other.vim_state(), std::nullopt, "ci\"x<Esc>"),
           "a change through an object records the inserted text as well");
    vim_replay(other, "6l.");
    expect(vim_body(other.frame()) == "a \"x\" c \"x\" e", ". replays a change through an object");
    Editing cancelled;
    open_vim_document(cancelled, "alpha beta gamma");
    EditorController &third = cancelled.controller();
    vim_replay(third, "x");
    vim_replay(third, "di<Esc>");
    expect(dot_record_is(third.vim_state(), std::nullopt, "x"),
           "a cancelled object leaves the last change alone");
}

// 回数が尽きた取消のキャレットと選択（Issue #99・ADR 0031 の補足）。ビープが後続の鍵を捨てる
// ので VISUAL の取消は 1 回の `:normal!` に乗らず、fixture にできない（Issue #87 の教訓）。
// 固定 Vim を区切って測った値をここで engine に固定する（`out/issue99-oracle/probe*.txt`）。
void verify_vim_text_object_residuals()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma");
    EditorController &controller = editing.controller();
    vim_replay(controller, "2lv9iwy");
    expect(controller.vim_state().unnamed_register.text == "alpha beta gamma",
           "a count that runs out leaves the selection it managed to build (measured)");
    vim_replay(controller, "<Esc>02lvl9iwy");
    expect(controller.vim_state().unnamed_register.text == "pha beta gamma",
           "a forward selection keeps its anchor when the count runs out");
    vim_replay(controller, "<Esc>04lvhh9iwy");
    expect(controller.vim_state().unnamed_register.text == "alpha",
           "a backward selection keeps its anchor and the caret stops at the start of the text");
    vim_replay(controller, "<Esc>09lvhh9iwy");
    expect(controller.vim_state().unnamed_register.text == "alpha beta",
           "the backward walk crosses as many units as it can before it gives up");
    vim_replay(controller, "<Esc>02lV9iwy");
    expect(controller.vim_state().unnamed_register.kind == VimRegisterKind::lines,
           "a cancelled object in linewise VISUAL stays linewise");
    vim_replay(controller, "<Esc>0x");
    vim_replay(controller, "d9iw");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "a cancelled object moves the caret but is not a change for .");
    Editing blocks;
    open_vim_document(blocks, "if {a; b;} else");
    EditorController &other = blocks.controller();
    vim_replay(other, "5lvl9i{y");
    expect(other.vim_state().unnamed_register.text == "; ",
           "a block that is not found leaves the selection and the caret alone");
}

// 引用符の対の選び方は選択の向きで変わる（決定 6・Issue #111 で実測）。取消はビープが後続の鍵を
// 捨てて 1 回の `:normal!` に乗らないので（Issue #87）、fixture に採れないぶんをここで固定する。
void verify_vim_text_object_quote_pairs()
{
    Editing editing;
    open_vim_document(editing, "a \"bb\" c \"dd\" e");
    EditorController &controller = editing.controller();
    vim_replay(controller, "3lvhi\"y");
    expect(controller.vim_state().unnamed_register.text == "\"b",
           "a backward selection on the first quote of the line cancels (measured on Vim 9.1)");
    vim_replay(controller, "<Esc>02lvhi\"y");
    expect(controller.vim_state().unnamed_register.text == " \"",
           "a backward selection with no quote before the caret cancels");
    vim_replay(controller, "<Esc>012lvli\"y");
    expect(controller.vim_state().unnamed_register.text == "\" ",
           "a forward selection with no pair after the caret cancels");
    Editing odd;
    open_vim_document(odd, "it's a 'quoted' word");
    EditorController &other = odd.controller();
    vim_replay(other, "16lvhi'y");
    expect(other.vim_state().unnamed_register.text == " w",
           "a backward selection past the last unpaired quote cancels");
    Editing across;
    open_vim_document(across, "x \"aa\" y\nz \"bb\" w");
    EditorController &two_lines = across.controller();
    vim_replay(two_lines, "j7lvki\"y");
    expect(two_lines.vim_state().unnamed_register.text == "y\nz \"bb\" w",
           "a selection whose ends are on two lines cancels (quotes are read within one line)");
}

void verify_vim_text_object_fixtures()
{
    constexpr std::array<std::string_view, 6> boundaries{
        "replace-char-one",    "replace-char-count", "char-search-f-count",
        "char-search-t-first", "line-jump-gg-count", "dot-remove-word"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("text-object-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 272,
           "the scope replays 266 text-object fixtures and 6 shared next-key boundaries");
}
} // namespace

void verify_vim_text_object_contracts()
{
    verify_vim_text_object_waiting();
    verify_vim_text_object_cancellation();
    verify_vim_text_object_visual();
    verify_vim_text_object_history();
    verify_vim_text_object_dot();
    verify_vim_text_object_residuals();
    verify_vim_text_object_quote_pairs();
}

void verify_vim_text_object_scope()
{
    verify_vim_text_object_fixtures();
    verify_vim_text_object_contracts();
}
} // namespace nenenib::tests
