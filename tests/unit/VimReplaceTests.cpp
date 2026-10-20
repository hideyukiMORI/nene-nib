// scope `--vim-replace` の単体テスト（ADR 0042 決定 2）。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
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
#include "TextPosition.hpp"
#include "VimCharacter.hpp"
#include "VimColumnWish.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimNoEffect.hpp"
#include "VimPrefix.hpp"
#include "VimRegisterKind.hpp"
#include "VimRemoveRange.hpp"
#include "VimReplaceRange.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VimWantedColumn.hpp"
#include "VirtualColumn.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
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
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::Selection;
using nenenib::core::SelectionPresence;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::vim_step;
using nenenib::core::VimCharacter;
using nenenib::core::VimColumnWish;
using nenenib::core::VimCount;
using nenenib::core::VimEditorView;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimNoEffect;
using nenenib::core::VimPrefix;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimSelect;
using nenenib::core::VimState;
using nenenib::core::VimViewport;
using nenenib::core::VirtualColumn;

void verify_vim_replace_waiting()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nx\nabcdef").value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 3, 0}};
    VimState initial = empty_vim_state();
    initial.count = nenenib::core::VimCount{2};
    initial.unnamed_register = nenenib::core::VimRegisterSnapshot::from(
        nenenib::core::VimRegister{"saved", VimRegisterKind::characters});
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    expect(waits_for_prefix(waiting.next, VimPrefix::r) && waiting.next.count == initial.count &&
               std::holds_alternative<nenenib::core::VimNoEffect>(waiting.effect),
           "r waits for one target while keeping the explicit count");
    const auto replaced = vim_step(waiting.next, view, VimKey{VimCharacter{U'0'}});
    expect(std::holds_alternative<nenenib::core::VimReplaceRange>(replaced.effect),
           "a digit after r is its target character");
    expect(replaced.next.mode == VimMode::normal && !replaced.next.input_wait.has_value() &&
               !replaced.next.count.has_value() &&
               replaced.next.unnamed_register.value().text == "saved",
           "replacement consumes transient input without overwriting the register");
    initial.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    // Vim の nv_replace と同じく、回数が行に収まらなくても置き換える文字を 1 鍵読んでから
    // 失敗にする（ADR 0145 の z の接頭辞・#634）。範囲を作らないので本文は確保しない。
    const auto too_many = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    expect(waits_for_prefix(too_many.next, VimPrefix::r) &&
               std::holds_alternative<nenenib::core::VimNoEffect>(too_many.effect) &&
               !too_many.failure.has_value(),
           "a replacement that exceeds the line still waits for its target");
    const auto next = vim_step(too_many.next, view, VimKey{VimCharacter{U'x'}});
    expect(std::holds_alternative<nenenib::core::VimNoEffect>(next.effect) &&
               next.failure == std::optional{nenenib::core::VimRepeatFailure::refused} &&
               !next.next.input_wait.has_value() && !next.next.count.has_value(),
           "the target after insufficient characters is consumed and fails without allocating");
}

void verify_vim_replace_cancellation()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nx\nabcdef").value();
    const Selection selected{Offset{0}, Offset{3}};
    const VimEditorView view{buffer, selected, VimViewport{LineNumber{1}, 3, 0}};
    VimState initial = empty_vim_state();
    initial.mode = VimMode::visual;
    initial.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    // <CR> はここに並ばない。VISUAL の r<CR> は選んだ各文字を literal CR に置き換える
    // （ADR 0036 の決定 5・fixture literal-cr-visual-*）。
    for (const auto key : {VimSpecialKey::escape, VimSpecialKey::backspace,
                           VimSpecialKey::arrow_left, VimSpecialKey::home})
    {
        const auto cancelled = vim_step(waiting.next, view, VimKey{key});
        expect(cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value() &&
                   cancelled.next.wanted_column == initial.wanted_column &&
                   std::holds_alternative<nenenib::core::VimNoEffect>(cancelled.effect),
               "cancellation and unsupported targets preserve the visual selection and wish");
    }
    const auto down = vim_step(vim_step(waiting.next, view, VimKey{VimSpecialKey::escape}).next,
                               view, VimKey{VimCharacter{U'j'}});
    expect(std::get<VimSelect>(down.effect).selection == Selection{Offset{0}, Offset{6}},
           "the next independent motion still extends the preserved selection to line end");
}

void verify_vim_replace_history()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\ncdef\r\ngh";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "jlr界rZ");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\ncZef\r\ngh", "replacement preserves CRLF bytes");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nc界ef\r\ngh", "each r is a separate undo unit");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "undo restores the original code point bytes");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nc界ef\r\ngh", "redo restores the replacement");
}

void verify_vim_replace_newline()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "abcdef\r\nend";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "l3r<CR>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a\r\nef\r\nend", "counted r Enter inserts one CRLF");
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "the replacement caret accounts for the document newline width");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores the entire replaced range");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a\r\nef\r\nend", "redo preserves the inserted CRLF");
}

void verify_vim_replace_visual_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\ncdef\r\ngh";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "lvjr界");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a界\r\n界界ef\r\ngh",
           "visual replacement changes characters while preserving document newlines");
    expect(controller.vim_state().mode == VimMode::normal &&
               controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "visual replacement finishes in NORMAL and collapses its selection");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores a multiline visual replace");
    vim_replay(controller, "r");
    expect(waits_for_prefix(controller.vim_state(), VimPrefix::r), "replacement is awaiting input");
    applied(controller, SelectEditMode{EditMode::ordinary});
    applied(controller, SelectEditMode{EditMode::vim});
    expect(!controller.vim_state().input_wait.has_value(), "mode switching discards the r wait");
}

// `J` `gJ` は CRLF の文書の改行を落としてつなぎ、つないだ全体が undo 1 単位（ADR 0015 の決定 9）。
// `.` の記録は回数つきの `J` で、レジスタは書かない。
void verify_vim_join_document_newlines()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\n  cd\r\nef\r\nx";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3J");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab cd ef\r\nx", "3J joins three CRLF lines into one");
    expect(caret_at(controller.frame(), 1, 6), "J rests on the space before the last joined line");
    expect(dot_record_is(controller.vim_state(), VimCount{3}, "J"), "`.` records the counted J");
    const VimState joined = controller.vim_state();
    expect(joined.unnamed_register.value().kind == VimRegisterKind::uninitialized,
           "J writes no register");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores every joined line");
    expect(caret_at(controller.frame(), 1, 1), "undo returns to where J was pressed");
    vim_replay(controller, "gJ");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab  cd\r\nef\r\nx",
           "gJ keeps the blanks of the next line");
}

// 大小文字のオペレータは CRLF の改行を保ち、1 回の変更が undo 1 単位（ADR 0015 の決定 10）。
// 矩形 VISUAL の `~` `u` `U` `>` `<` は鍵を拒んで矩形のまま残る（#770）。
void verify_vim_case_document_newlines()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "aB\r\ncD\r\nx";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "g~j");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "Ab\r\nCd\r\nx", "g~j toggles two CRLF lines");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "g~j"), "`.` records g~j");
    vim_replay(controller, "jgUU");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "Ab\r\nCD\r\nx", "gUU uppers one line");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "Ab\r\nCd\r\nx", "each case change is its own undo unit");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "the second undo restores the original case");
    for (const std::string_view keys : {"~", "u", "U", ">", "<"})
    {
        vim_replay(controller, "gg<C-v>j");
        vim_replay(controller, keys);
        expect(controller.vim_state().mode == VimMode::visual_block,
               "a blockwise case or shift key is refused and the block stays");
        vim_replay(controller, "<Esc>");
        applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
        expect(editing.files().written() == original, "a refused blockwise key changes nothing");
    }
}

// `>` `<` は CRLF の改行を保ち、`.` の 2 回目は別の undo 単位（ADR 0015 の決定 11）。VISUAL
// の回数は 記録の頭に残り、大きすぎる回数は書かずに失敗する。
void verify_vim_shift_document_newlines()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "a\r\nb\r\nc";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, ">j");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "\ta\r\n\tb\r\nc", ">j indents two CRLF lines");
    vim_replay(controller, "j.");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "\ta\r\n\t\tb\r\n\tc", "`.` repeats >j from the caret");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "\ta\r\n\tb\r\nc", "the repeat is its own undo unit");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "the second undo restores the original indent");
    vim_replay(controller, "Vj2>");
    const VimState shifted = controller.vim_state();
    expect(dot_record_is(shifted, std::nullopt, "2>") && shifted.last_change.has_value() &&
               shifted.last_change.value().visual.has_value(),
           "a VISUAL shift records its count ahead of the key with the selection size");
    vim_replay(controller, "uV18446744073709551615>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "an indent too wide to write changes nothing");
}
} // namespace

void verify_vim_replace_contracts()
{
    verify_vim_replace_waiting();
    verify_vim_replace_cancellation();
    verify_vim_replace_history();
    verify_vim_replace_newline();
    verify_vim_replace_visual_undo();
    verify_vim_join_document_newlines();
    verify_vim_case_document_newlines();
    verify_vim_shift_document_newlines();
}

void verify_vim_replace_scope()
{
    constexpr std::array<std::string_view, 10> boundaries{"p-puts-the-character-after-the-caret",
                                                          "p-puts-the-line-below",
                                                          "capital-p-puts-the-line-above",
                                                          "capital-p-of-a-line-on-the-first-line",
                                                          "japanese-p-after-a-code-point",
                                                          "open-line-count-below",
                                                          "open-line-count-above",
                                                          "open-line-above",
                                                          "char-search-visual-y-f-range",
                                                          "line-jump-visual-gg-yank"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("replace-char-") || fixture.name.starts_with("literal-cr-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 76, "the scope replays 37 replacements, 29 literal-CR cases and 10 "
                           "shared-path boundaries");
    verify_vim_replace_contracts();
    verify_vim_character_search_waiting();
    verify_vim_line_jump_waiting();
}
} // namespace nenenib::tests
