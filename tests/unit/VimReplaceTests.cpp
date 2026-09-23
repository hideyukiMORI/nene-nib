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
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 3}};
    VimState initial = empty_vim_state();
    initial.count = nenenib::core::VimCount{2};
    initial.unnamed_register = {"saved", VimRegisterKind::characters};
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    expect(waits_for_prefix(waiting.next, VimPrefix::r) && waiting.next.count == initial.count &&
               std::holds_alternative<nenenib::core::VimNoEffect>(waiting.effect),
           "r waits for one target while keeping the explicit count");
    const auto replaced = vim_step(waiting.next, view, VimKey{VimCharacter{U'0'}});
    expect(std::holds_alternative<nenenib::core::VimReplaceRange>(replaced.effect),
           "a digit after r is its target character");
    expect(replaced.next.mode == VimMode::normal && !replaced.next.input_wait.has_value() &&
               !replaced.next.count.has_value() && replaced.next.unnamed_register.text == "saved",
           "replacement consumes transient input without overwriting the register");
    initial.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    const auto too_many = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    expect(!too_many.next.input_wait.has_value() && !too_many.next.count.has_value() &&
               std::holds_alternative<nenenib::core::VimNoEffect>(too_many.effect),
           "a replacement that exceeds the line fails before waiting or allocating");
    const auto next = vim_step(too_many.next, view, VimKey{VimCharacter{U'x'}});
    expect(std::holds_alternative<nenenib::core::VimRemoveRange>(next.effect),
           "the next command after insufficient characters is not swallowed as an r target");
}

void verify_vim_replace_cancellation()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nx\nabcdef").value();
    const Selection selected{Offset{0}, Offset{3}};
    const VimEditorView view{buffer, selected, VimViewport{LineNumber{1}, 3}};
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
} // namespace

void verify_vim_replace_contracts()
{
    verify_vim_replace_waiting();
    verify_vim_replace_cancellation();
    verify_vim_replace_history();
    verify_vim_replace_newline();
    verify_vim_replace_visual_undo();
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
