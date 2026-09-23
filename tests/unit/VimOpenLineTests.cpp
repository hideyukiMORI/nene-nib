// scope `--vim-open-lines` の単体テスト（ADR 0042 決定 2）。
#include "Column.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Edit.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::application::CommitText;
using nenenib::application::ComposeText;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::SaveDocument;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::absorbed;
using nenenib::core::Column;
using nenenib::core::Edit;
using nenenib::core::EditMode;
using nenenib::core::HistoryDirection;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::SelectionAnchoring;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimState;

void verify_vim_open_line_undo()
{
    verify_open_line_round_trip("aa\r\nbb", "3oあ<CR>😀<Esc>",
                                "aa\r\nあ\r\n😀\r\nあ\r\n😀\r\nあ\r\n😀\r\nbb");
    verify_open_line_round_trip("aa\r\nbb", "3Oa<BS>日<Esc>", "日\r\n日\r\n日\r\naa\r\nbb");
    verify_open_line_round_trip("aa\nbb", "j3O<BS><BS>X<Esc>", "aX\nbb");
    verify_open_line_round_trip("aa\nbb", "3o<BS>X<Esc>", "aaXXX\nbb");
    verify_open_line_round_trip("aa\n", "GoX<Esc>", "aa\n\nX");
    verify_open_line_round_trip("", "3O<Esc>", "\r\n\r\n\r\n");
}

void verify_vim_open_line_intermediate()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("aa\nbb")});
    applied(controller, VisibleLines{2});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "G3o");
    const auto frame = controller.frame();
    expect(frame.total_lines == 3 && frame.vim_mode == VimMode::insert,
           "a counted open starts one empty line and enters INSERT immediately");
    expect(frame.caret.position == TextPosition{LineNumber{3}, Column{1}} &&
               frame.first_visible == LineNumber{2},
           "opening below EOF follows the caret");
    applied(controller, ComposeText{composed_of("に", {}, 3)});
    expect(controller.frame().composition.has_value(), "opened INSERT accepts composition");
    applied(controller, CommitText{"日本😀"});
    expect(controller.frame().total_lines == 3, "typing does not expand the count yet");
    vim_replay(controller, "<Esc>");
    expect(controller.frame().caret.position == TextPosition{LineNumber{5}, Column{3}},
           "Esc expands the committed UTF-8 input and rests on the last character");
    expect(whole_vim_body(controller) == "aa\nbb\n日本😀\n日本😀\n日本😀",
           "IME commit and direct characters share the insertion record");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "aa\nbb", "IME and opening form one undo unit");
}

void verify_vim_open_line_switch()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3oX");
    applied(controller, SelectEditMode{EditMode::ordinary});
    expect(!controller.vim_state().insert_repeat.has_value(), "ordinary mode cancels repetition");
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "iY<Esc>");
    expect(whole_vim_body(controller) == "\nYX", "a later INSERT cannot replay the old count");
}

void verify_history_inner_absorbing()
{
    const Edit typed{Offset{5}, "old", "ab\n"};
    expect(absorbed_edit(typed, Edit{Offset{5}, "", "X"}) == Edit{Offset{5}, "old", "Xab\n"},
           "inserting at the start preserves the old removal");
    expect(absorbed_edit(typed, Edit{Offset{6}, "b", "YZ"}) == Edit{Offset{5}, "old", "aYZ\n"},
           "a replacement inside the inserted span is composed into that edit");
    expect(absorbed_edit(typed, Edit{Offset{5}, "a", ""}) == Edit{Offset{5}, "old", "b\n"},
           "deleting before the trailing open-line newline stays in the same unit");
    expect(!absorbed(typed, Edit{Offset{7}, "\nx", ""}).has_value(),
           "a removal across the end cannot be absorbed as an inner edit");
}
} // namespace

// 開行と反復は既存のINSERT・保存・undoの一単位（ADR 0028）。
void verify_open_line_round_trip(std::string_view initial, std::string_view keys,
                                 std::string_view expected)
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(initial)});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, keys);
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == expected, "open-line saves the expected original bytes");
    expect(controller.vim_state().mode == VimMode::normal &&
               !controller.vim_state().insert_repeat.has_value(),
           "Esc clears the repeat session");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == initial, "one undo restores the document before opening");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == expected, "one redo restores opening and all insertions");
}

void verify_vim_open_line_movement()
{
    constexpr std::array<VimSpecialKey, 8> keys{
        VimSpecialKey::arrow_left, VimSpecialKey::arrow_right, VimSpecialKey::arrow_up,
        VimSpecialKey::arrow_down, VimSpecialKey::home,        VimSpecialKey::end,
        VimSpecialKey::page_up,    VimSpecialKey::page_down};
    for (const VimSpecialKey key : keys)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        applied(controller, SelectEditMode{EditMode::vim});
        vim_replay(controller, "3Oabc");
        applied(controller, VimKeyPress{VimKey{key}});
        expect(!controller.vim_state().insert_repeat.has_value(),
               "every INSERT movement cancels open-line repetition, including no-op moves");
        vim_replay(controller, "Z<Esc>u");
        expect(whole_vim_body(controller) == "abc\n", "movement seals the earlier insertion");
        vim_replay(controller, "u");
        expect(whole_vim_body(controller).empty(), "the earlier undo also removes the open line");
    }
}

void verify_vim_open_line_fixtures()
{
    std::size_t opened = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("open-line-"))
        {
            verify_vim_fixture(fixture);
            ++opened;
        }
        if (fixture.name.find("puts") != std::string_view::npos ||
            fixture.name == "i-inserts-before-the-caret" ||
            fixture.name == "a-inserts-after-the-caret")
        {
            verify_vim_fixture(fixture);
        }
    }
    expect(opened == 38, "all 38 measured open-line fixtures were replayed");
}

void verify_vim_open_line_capacity()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("aa\nbb")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    const std::string count = std::to_string(std::numeric_limits<std::size_t>::max());
    vim_replay(controller, count + "oX<Esc>");
    expect(whole_vim_body(controller) == "aa\nX\nbb",
           "overflowing repetition preserves the initial insertion without expansion");
    expect(controller.vim_state().mode == VimMode::normal &&
               !controller.vim_state().insert_repeat.has_value(),
           "a refused repetition still exits INSERT and clears its count");
    vim_replay(controller, "uyy" + count + "p");
    expect(whole_vim_body(controller) == "aa\nbb", "put shares the checked repetition size");
    vim_replay(controller, "3o<BS><Esc>");
    expect(whole_vim_body(controller) == "aa\nbb", "empty repetition does not allocate or loop");
}

void verify_vim_open_line_external_input()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("aa\nbb")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3OX");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{2}}, SelectionAnchoring::collapse});
    expect(!controller.vim_state().insert_repeat.has_value(),
           "even a same-position click cancels repeat");
    vim_replay(controller, "Y<Esc>u");
    expect(whole_vim_body(controller) == "X\naa\nbb", "a click seals the prior opening");
    vim_replay(controller, "u3oX");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(!controller.vim_state().insert_repeat.has_value(), "Ctrl+Z cancels the undone session");
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller) == "aa\nbb", "Esc cannot replay input removed by Ctrl+Z");
    vim_replay(controller, "3oX");
    applied(controller, SelectAll{});
    expect(!controller.vim_state().insert_repeat.has_value(),
           "external selection cancels repetition");
}

void verify_vim_open_line_external_edit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3Oab");
    applied(controller, InsertText{"PASTE"});
    expect(!controller.vim_state().insert_repeat.has_value(),
           "unrecorded edit cannot leave stale replay text");
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller) == "abPASTE\n", "Esc preserves unrecorded input once");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "ab\n", "external edit has its own undo unit");
}

// 割り込みが捨てるものと保つもの（Issue #92）。範囲を決めるのは engine の純関数
// vim_interrupted で、controller は「どれが割り込みか」だけを決める（ARC-004）。
void verify_vim_interrupt_discards()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("alpha beta\nsecond line")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "x3Oab");
    expect(controller.vim_state().insert_repeat.has_value() &&
               controller.vim_state().recording.has_value(),
           "the counted open line is holding both a repetition and a half-built dot recording");
    applied(controller, SelectAll{});
    expect(controller.vim_state().mode == VimMode::insert &&
               !controller.vim_state().insert_repeat.has_value() &&
               !controller.vim_state().recording.has_value(),
           "an interruption drops the repetition and the recording it cannot replay, not the mode");
    expect(controller.vim_state().last_change.has_value(),
           "the change that was already confirmed is still what the dot repeats");

    // 割り込みは鍵ではないので、engine の純関数を直に呼んで捨てる範囲を確かめる。
    Editing pending;
    EditorController &waiting = pending.controller();
    pending.files().hold(Bytes{std::string("alpha beta\nsecond line")});
    applied(waiting, OpenDocument{sample_path()});
    applied(waiting, SelectEditMode{EditMode::vim});
    vim_replay(waiting, "fa2d3f");
    const VimState &before = waiting.vim_state();
    expect(before.count.has_value() && before.pending.has_value() &&
               before.input_wait.has_value() && before.last_character_search.has_value(),
           "the operator is pending with a count and is waiting for one more key");
    const VimState after = nenenib::core::vim_interrupted(before);
    expect(!after.count.has_value() && !after.pending.has_value() && !after.input_wait.has_value(),
           "the pure function drops the count, the pending operator and the awaited key");
    expect(after.mode == before.mode && after.last_character_search.has_value() &&
               after.last_character_search.value().target == U'a',
           "and keeps the mode and the remembered character search");
}

void verify_vim_open_line_contracts()
{
    verify_vim_open_line_undo();
    verify_vim_open_line_intermediate();
    verify_vim_open_line_movement();
    verify_vim_open_line_switch();
    verify_vim_interrupt_discards();
    verify_vim_open_line_external_input();
    verify_vim_open_line_external_edit();
    verify_vim_open_line_capacity();
    verify_history_inner_absorbing();
}

void verify_vim_open_line_scope()
{
    verify_vim_open_line_contracts();
    verify_vim_open_line_fixtures();
    verify_history_absorbing();
    verify_history_coalescing();
    verify_vim_insert_undo_unit();
    verify_vim_change_undo_unit();
    verify_vim_insert_motion_breaks_the_unit();
    verify_vim_insert_page_move_breaks_undo();
    verify_vim_put_line_endings();
}
} // namespace nenenib::tests
