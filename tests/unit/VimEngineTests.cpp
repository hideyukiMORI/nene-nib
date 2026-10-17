// Vim エンジンの単体テスト（ADR 0012・ADR 0042）。既定実行と scope が共有する。
#include "CaretMotion.hpp"
#include "CaretShape.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "ModeLabel.hpp"
#include "MoveCaret.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScrollLines.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimCount.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimInsertRepeat.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimKeySource.hpp"
#include "VimLayoutView.hpp"
#include "VimLineRows.hpp"
#include "VimMacroRecording.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNavigate.hpp"
#include "VimNewLine.hpp"
#include "VimNoEffect.hpp"
#include "VimPendingOperator.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRegisterSelection.hpp"
#include "VimRegisterSnapshot.hpp"
#include "VimRemoveLines.hpp"
#include "VimRepeatFailure.hpp"
#include "VimRepeatRecord.hpp"
#include "VimRowSpan.hpp"
#include "VimScrollUnit.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimSwitchTab.hpp"
#include "VimTabs.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VimVisualRange.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"
#include "VimWordWalk.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnIndex.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::ClipboardAction;
using nenenib::application::ClipboardOperation;
using nenenib::application::MoveCaret;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::SaveDocument;
using nenenib::application::ScrollLines;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::CaretMotion;
using nenenib::core::CaretShape;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::is_empty;
using nenenib::core::LineNumber;
using nenenib::core::mode_label;
using nenenib::core::next_code_point;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::Selection;
using nenenib::core::SelectionAnchoring;
using nenenib::core::SelectionPresence;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::vim_first_non_blank;
using nenenib::core::vim_next_word;
using nenenib::core::vim_previous_word;
using nenenib::core::vim_previous_word_end;
using nenenib::core::vim_resting_caret;
using nenenib::core::vim_same_line_and_column;
using nenenib::core::vim_step;
using nenenib::core::vim_visual_range;
using nenenib::core::vim_word_end;
using nenenib::core::VimCharacter;
using nenenib::core::VimEditorView;
using nenenib::core::VimInsertRepeat;
using nenenib::core::VimKey;
using nenenib::core::VimKeySource;
using nenenib::core::VimLayoutView;
using nenenib::core::VimLineRows;
using nenenib::core::VimMacroRecording;
using nenenib::core::VimMode;
using nenenib::core::VimMoveTo;
using nenenib::core::VimNavigate;
using nenenib::core::VimNewLine;
using nenenib::core::VimNoEffect;
using nenenib::core::VimPendingOperator;
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimRegisterSelection;
using nenenib::core::VimRegisterSnapshot;
using nenenib::core::VimRepeatRecord;
using nenenib::core::VimSelect;
using nenenib::core::VimState;
using nenenib::core::VimTabs;
using nenenib::core::VimViewport;
using nenenib::core::VimWordClass;
using nenenib::core::VimWordEndStop;
using nenenib::core::VimWordStop;
using nenenib::core::VimWordWalk;

// キャレットの桁をバイトで測り直す。Vim の col('.') はバイト位置で、表示値の桁は code point。
// oracle の本文は LF で区切られ、行の中の '\r' は文字である（ADR 0036 の決定 4）。TextBuffer へ
// 入れ直すと先頭の行の末尾の '\r' を改行の形の判別が CRLF と読むので、ここは '\n' だけで数える。
[[nodiscard]] std::size_t vim_byte_column(const std::string &body, const TextPosition &caret)
{
    std::size_t start = 0;
    for (std::size_t line = 1; line < caret.line.value; ++line)
    {
        const std::size_t found = body.find('\n', start);
        expect(found != std::string::npos, "the replayed body holds the caret's line");
        if (found == std::string::npos)
        {
            return 0;
        }
        start = found + 1;
    }
    const std::size_t stop = std::min(body.find('\n', start), body.size());
    const std::string_view content = std::string_view(body).substr(start, stop - start);
    std::size_t byte = 0;
    for (std::size_t step = 1; step < caret.column.value && byte < content.size(); ++step)
    {
        byte = next_code_point(content, Offset{byte}).value;
    }
    return byte + 1;
}

void verify_vim_fixtures()
{
    expect(nenenib::tests::vim_fixtures.size() >= 250,
           "the oracle wrote at least the fixtures the Issues ask for (#22 / #43 / #53)");
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        verify_vim_fixture(fixture);
    }
}

// fixture の再生は :normal! と同じに失敗した鍵の後ろを流さず、打った鍵は流す（Issue #230）。
void verify_vim_normal_stops_at_failure()
{
    Editing normal;
    open_vim_document(normal, "abc");
    vim_normal(normal.controller(), "hx");
    expect(whole_vim_body(normal.controller()) == "abc",
           "under :normal! a failed h drops the x after it");
    Editing typed;
    open_vim_document(typed, "abc");
    vim_replay(typed.controller(), "hx");
    expect(whole_vim_body(typed.controller()) == "bc", "a typed x still runs after a failed h");
}

void verify_vim_key_notation()
{
    const auto keys = vim_keys_of("i<Esc><CR><BS><C-r><Home><End>あ");
    expect(keys.size() == 8, "every name and every code point becomes one key");
    expect(std::get<VimCharacter>(keys.at(0)) == VimCharacter{U'i'}, "a plain letter");
    expect(std::get<VimSpecialKey>(keys.at(1)) == VimSpecialKey::escape, "<Esc>");
    expect(std::get<VimSpecialKey>(keys.at(2)) == VimSpecialKey::enter, "<CR>");
    expect(std::get<VimSpecialKey>(keys.at(3)) == VimSpecialKey::backspace, "<BS>");
    expect(std::get<VimSpecialKey>(keys.at(4)) == VimSpecialKey::control_r, "<C-r>");
    expect(std::get<VimSpecialKey>(keys.at(5)) == VimSpecialKey::home, "<Home>");
    expect(std::get<VimSpecialKey>(keys.at(6)) == VimSpecialKey::end, "<End>");
    expect(std::get<VimCharacter>(keys.at(7)) == VimCharacter{U'あ'},
           "a multibyte code point is one key");
}

void verify_vim_mode_labels()
{
    expect(mode_label(EditMode::ordinary, VimMode::normal) == "通常", "the ordinary label");
    expect(mode_label(EditMode::ordinary, VimMode::insert) == "通常",
           "the Vim mode does not show through in ordinary mode");
    expect(mode_label(EditMode::vim, VimMode::normal) == "NORMAL", "the NORMAL label");
    expect(mode_label(EditMode::vim, VimMode::insert) == "INSERT", "the INSERT label");
    expect(mode_label(EditMode::vim, VimMode::visual) == "VISUAL", "the VISUAL label");
    expect(mode_label(EditMode::vim, VimMode::visual_line) == "VISUAL LINE",
           "the VISUAL LINE label");
    expect(mode_label(EditMode::ordinary, VimMode::visual) == "通常",
           "VISUAL does not show through in ordinary mode either");
}

void verify_vim_caret_shapes()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(vim.caret.shape == CaretShape::block, "NORMAL draws a block");
    expect(vim.mode_label == "NORMAL", "NORMAL names itself on the status bar");
    const auto inserting = controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U'i'}}});
    expect(inserting.caret.shape == CaretShape::bar, "INSERT draws a bar");
    expect(inserting.mode_label == "INSERT", "INSERT names itself on the status bar");
    const auto back = controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::escape}});
    expect(back.caret.shape == CaretShape::block, "Esc brings the block back");
    const auto ordinary = controller.apply_frame(SelectEditMode{EditMode::ordinary});
    expect(ordinary.caret.shape == CaretShape::bar && ordinary.mode_label == "通常",
           "the ordinary mode takes its own caret and label back");
}

// VISUAL では描く選択と Ctrl+C / Ctrl+X の範囲が vim_visual_range と同じ 1 本
// （ADR 0018 の決定 5 の強制）。fixture は本文とレジスタしか見ないので、ここで面を測る。
void verify_vim_visual_selection_and_clipboard()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("abc\ndef")});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "vl");
    const auto characters = controller.frame();
    expect(characters.mode_label == "VISUAL" && characters.caret.shape == CaretShape::block,
           "VISUAL names itself on the status bar and keeps the block caret");
    expect(characters.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "the drawn selection takes the character under the far end");
    applied(controller, ClipboardAction{ClipboardOperation::copy});
    expect(editing.clipboard().read().value() == "ab", "Ctrl+C copies that same range");
    vim_replay(controller, "V");
    const auto lines = controller.frame();
    expect(lines.mode_label == "VISUAL LINE", "V switches the kind without losing the selection");
    expect(lines.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{4}},
           "the linewise selection covers the whole line");
    applied(controller, ClipboardAction{ClipboardOperation::copy});
    expect(editing.clipboard().read().value() == "abc", "and Ctrl+C takes the whole line");
    vim_replay(controller, "<Esc>");
    const auto back = controller.frame();
    expect(back.mode_label == "NORMAL", "Esc goes back to NORMAL");
    expect(back.lines.at(0).selection.presence == SelectionPresence::absent,
           "and nothing is drawn as selected any more");
    expect(back.caret.position == TextPosition{LineNumber{1}, Column{2}},
           "the caret stays where VISUAL left it");
}

// Vim に入るときのキャレットは文字の上へ寄る。通常へ戻ると保留中の回数とオペレータは消える。
void verify_vim_mode_entry()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("abc")});
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(
        controller.apply_frame(MoveCaret{CaretMotion::line_end, SelectionAnchoring::collapse}));
    const auto entered = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(entered.caret.position.column == Column{3},
           "entering Vim pulls the caret back onto the last character");
    vim_replay(controller, "2d");
    // 2d の 2 はオペレータが持ち、そのあとの 3 は新しい回数になる（ADR 0015 の決定 1）。
    expect(controller.vim_state().pending.has_value() && !controller.vim_state().count.has_value(),
           "the operator carries its own count and waits for the motion");
    vim_replay(controller, "3");
    expect(controller.vim_state().count.has_value(), "the motion's count is counted on its own");
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::ordinary}));
    expect(!controller.vim_state().count.has_value() && !controller.vim_state().pending.has_value(),
           "leaving Vim drops what was pending");
    expect(controller.vim_state().mode == VimMode::normal, "and the mode goes back to NORMAL");
}

// oracle の :normal! は 1 回の実行をまるごと 1 つの undo の単位にするので、i a I A の出入りが
// 単位を閉じることは fixture では測れない。ここだけ手で測る（ADR 0012 の決定 6）。
void verify_vim_undo_boundaries()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello")});
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "iab<Esc>ic<Esc>");
    expect(vim_body(controller.frame()) == "acbhello", "two inserts land where Vim puts them");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "abhello", "one undo takes back one insert");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "the second undo takes back the first insert");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "undo at the end of the history does nothing");
}

// fixture は LF だけ（Vim が CRLF を fileformat=dos として落とすので oracle に流せない）。
// CRLF の本文で CR が本文に残らないことは、保存したバイト列で手で測る（ADR 0012 の決定 9）。
// CRLF の文書は fixture にできない。`-S probe.vim input.txt` の順では `set binary` が間に合わず、
// 全部の行が CR で終わる入力を Vim は dos と読んで CR を落とし、1 行でも CR で終わらない行が
// あれば unix と読んで CR を文字として残すからである（ADR 0036 の決定 4 と決定 7・実測）。
// ここに並ぶのは engine 自身の答えで、同じ鍵の LF の fixture と対になっている。
// 再生の経路は fixture と同じ 1 本（verify_vim_fixture）を通す（ARC-001）。
void verify_vim_crlf_documents()
{
    constexpr std::array<VimFixture, 10> contracts{{
        {"crlf-line-jump-G", "a\r\n  b\r\nc", "2G", "a\n  b\nc", 2, 3, "", "", std::nullopt},
        {"crlf-open-line-below", "aa\r\nbb", "oX<Esc>", "aa\nX\nbb", 2, 1, "", "", std::nullopt},
        {"crlf-open-line-above", "aa\r\nbb", "jOX<Esc>", "aa\nX\nbb", 2, 1, "", "", std::nullopt},
        {"crlf-visual-wanted", "abcd\r\nx\r\nabcdef\r\nTAIL", "$vjjd", "abcTAIL", 1, 4,
         "d\nx\nabcdef\n", "v", std::nullopt},
        {"crlf-visual-yank", "  abcdef\r\nx\r\nTAIL", "$Vjy", "  abcdef\nx\nTAIL", 1, 1,
         "  abcdef\nx\n", "V", std::nullopt},
        {"crlf-replace-char", "abcd\r\nefgh", "l2r<CR>", "a\nd\nefgh", 2, 1, "", "", std::nullopt},
        {"crlf-replace-char-visual", "ab\r\ncdef\r\ngh", "lvjr\u754c",
         "a\u754c\n\u754c\u754cef\ngh", 1, 2, "", "", std::nullopt},
        {"crlf-dot-change-word", "ab cd\r\nef gh", "cwZZ<Esc>j0.", "ZZ cd\nZZ gh", 2, 2, "ef", "v",
         std::nullopt},
        {"crlf-dot-open-below", "ab\r\ncd", "ofoo<Esc>.", "ab\nfoo\nfoo\ncd", 3, 3, "", "",
         std::nullopt},
        {"crlf-dot-remove-line", "ab\r\ncd\r\nef", "dd.", "ef", 1, 1, "cd\n", "V", std::nullopt},
    }};
    for (const VimFixture &contract : contracts)
    {
        verify_vim_fixture(contract);
    }
}

// 窓が送れる鍵のうち、fixture の記法に無いもの（Tab・矢印）と、NORMAL では効かない鍵。
void verify_vim_other_keys()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\ntwo")});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    // Backspace は行をまたぐ h（ADR 0049）で、fixture の `space-bs-*` が守る。
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::enter}}));
    expect(vim_body(controller.frame()) == "one\ntwo", "Enter does not edit in NORMAL");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_down}}));
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_right}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{2}},
           "the arrows move like j and l");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_up}}));
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_left}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{1}},
           "the arrows move like k and h");
    vim_replay(controller, "i");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U'\t'}}}));
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::control_r}}));
    expect(vim_body(controller.frame()) == "\tone\ntwo", "Tab inserts a tab and Ctrl-r is ignored");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_down}}));
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_right}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{3}},
           "the arrows still move in INSERT");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_up}}));
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_left}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}},
           "and up and left too");
}

// core の純関数を直に測る。fixture は EditorController を通るので、端の値はここで押さえる。
void verify_vim_word_motions()
{
    const auto text = TextBuffer::from_utf8("alpha beta\n\ngamma");
    expect(text.has_value(), "the sample buffer parses");
    const auto &buffer = text.value();
    expect(vim_next_word(buffer, Offset{0}, VimWordWalk{2, VimWordClass::word},
                         VimWordStop::across_lines)
                   .offset == Offset{11},
           "two w land on the empty line, which is a word of its own");
    expect(vim_next_word(buffer, Offset{6}, VimWordWalk{1, VimWordClass::word},
                         VimWordStop::at_line_end)
                   .offset == Offset{10},
           "an operator's w stops at the end of the line it started on");
    expect(vim_next_word(buffer, Offset{12}, VimWordWalk{3, VimWordClass::word},
                         VimWordStop::across_lines)
                   .offset == Offset{17},
           "w runs out at the end of the buffer");
    expect(!vim_next_word(buffer, Offset{12}, VimWordWalk{3, VimWordClass::word},
                          VimWordStop::across_lines)
                .failure.has_value(),
           "w that runs out in the middle of a round does not fail (Vim's fwd_word returns OK)");
    expect(vim_previous_word(buffer, Offset{12}, VimWordWalk{2, VimWordClass::word}).offset ==
               Offset{6},
           "two b walk back over the empty line");
    const auto stuck = vim_previous_word(buffer, Offset{0}, VimWordWalk{1, VimWordClass::word});
    expect(stuck.offset == Offset{0} && stuck.failure.has_value(),
           "b at the start of the buffer stays and fails");
    // 回数の途中で本文の先頭に当たったら、着地まで動いて失敗する（Issue #224・Vim の return
    // FAIL）。
    const auto below_empty = TextBuffer::from_utf8("\nabc");
    expect(below_empty.has_value(), "the buffer below an empty first line parses");
    const auto counted_b =
        vim_previous_word(below_empty.value(), Offset{1}, VimWordWalk{2, VimWordClass::word});
    expect(counted_b.offset == Offset{0} && counted_b.failure.has_value(),
           "2b below an empty first line lands on it and fails");
    const auto counted_ge =
        vim_previous_word_end(below_empty.value(), Offset{3}, VimWordWalk{2, VimWordClass::word});
    expect(counted_ge.offset == Offset{0} && counted_ge.failure.has_value(),
           "2ge below an empty first line lands on it and fails");
    const auto exact_b =
        vim_previous_word(below_empty.value(), Offset{3}, VimWordWalk{2, VimWordClass::word});
    expect(exact_b.offset == Offset{0} && !exact_b.failure.has_value(),
           "2b that reaches the empty first line on its last word does not fail");
}

// 前向きの語の移動の失敗（Issue #226）。w は周の始めに本文の最後の文字にいたときだけ
// （Vim の fwd_word）、e は周のどの歩でも本文の終わりの先へ出ようとしたら（Vim の end_word）。
void verify_vim_forward_word_failures()
{
    const auto text = TextBuffer::from_utf8("abc d");
    expect(text.has_value(), "the buffer with a one-letter last word parses");
    const auto &buffer = text.value();
    const VimWordWalk nine{9, VimWordClass::word};
    const auto counted_w = vim_next_word(buffer, Offset{0}, nine, VimWordStop::across_lines);
    expect(counted_w.offset == Offset{5} && counted_w.failure.has_value(),
           "9w fails when a round starts on the last character");
    const auto exact_w = vim_next_word(buffer, Offset{0}, VimWordWalk{1, VimWordClass::word},
                                       VimWordStop::across_lines);
    expect(exact_w.offset == Offset{4} && !exact_w.failure.has_value(),
           "w onto the last character does not fail");
    const auto operator_w = vim_next_word(buffer, Offset{4}, VimWordWalk{1, VimWordClass::word},
                                          VimWordStop::at_line_end);
    expect(operator_w.offset == Offset{5} && operator_w.failure.has_value(),
           "an operator's w from the last character reaches the end and carries the failure");
    const auto counted_e =
        vim_word_end(buffer, Offset{0}, nine, VimWordEndStop::enter_the_next_word);
    expect(counted_e.offset == Offset{5} && counted_e.failure.has_value(),
           "9e fails once a round runs past the end");
    const auto exact_e = vim_word_end(buffer, Offset{0}, VimWordWalk{2, VimWordClass::word},
                                      VimWordEndStop::enter_the_next_word);
    expect(exact_e.offset == Offset{4} && !exact_e.failure.has_value(),
           "2e onto the last word end does not fail");
    const auto blanks = TextBuffer::from_utf8("abc  ");
    expect(blanks.has_value(), "the buffer with trailing blanks parses");
    const auto blank_e = vim_word_end(blanks.value(), Offset{2}, VimWordWalk{1, VimWordClass::word},
                                      VimWordEndStop::enter_the_next_word);
    expect(blank_e.failure.has_value(), "e fails when only blanks follow the last word");
    const auto blank_w = vim_next_word(
        blanks.value(), Offset{2}, VimWordWalk{1, VimWordClass::word}, VimWordStop::across_lines);
    expect(!blank_w.failure.has_value(), "w over trailing blanks runs out without failing");
}

void verify_vim_caret_rules()
{
    const auto text = TextBuffer::from_utf8("  alpha\n\n\t ");
    expect(text.has_value(), "the indented buffer parses");
    const auto &buffer = text.value();
    expect(vim_resting_caret(buffer, Offset{3}) == Offset{3}, "a caret on a character stays");
    expect(vim_resting_caret(buffer, Offset{7}) == Offset{6},
           "the position past the last character rests on it");
    expect(vim_resting_caret(buffer, Offset{8}) == Offset{8}, "an empty line rests at its start");
    expect(vim_first_non_blank(buffer, Offset{5}) == Offset{2}, "the indent is skipped");
    expect(vim_first_non_blank(buffer, Offset{8}) == Offset{8}, "an empty line has no non-blank");
    expect(vim_first_non_blank(buffer, Offset{9}) == Offset{10},
           "a line of blanks rests on its last character");
}

// Ctrl-r の戻り先の写し（ADR 0052 の決定 5）。
void verify_vim_redo_caret()
{
    const auto before = TextBuffer::from_utf8("ab\ncdef\n  ef");
    const auto after = TextBuffer::from_utf8("a\n xあ");
    expect(before.has_value() && after.has_value(), "the redo buffers parse");
    expect(vim_same_line_and_column(before.value(), Offset{3}, after.value()) == Offset{2},
           "redo keeps the line and the byte column");
    expect(vim_same_line_and_column(before.value(), Offset{6}, after.value()) == Offset{4},
           "a column inside a character goes to its start");
    expect(vim_same_line_and_column(before.value(), Offset{1}, after.value()) == Offset{1},
           "a column past the line stops at its end");
    expect(vim_same_line_and_column(before.value(), Offset{10}, after.value()) == Offset{3},
           "a line past the end goes to the first non-blank of the last line");
}

[[nodiscard]] std::string vim_lines(std::size_t count)
{
    std::string text;
    for (std::size_t line = 0; line < count; ++line)
    {
        if (line > 0)
        {
            text += '\n';
        }
        text += 'x';
    }
    return text;
}

void verify_vim_viewport_half_edges()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport sample parses");
    const auto &buffer = text.value();
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    state.count = nenenib::core::VimCount{999};
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{6}, 10, 0}};
    const auto down = vim_step(state, view, VimKey{VimSpecialKey::control_d});
    const auto &down_effect = std::get<VimNavigate>(down.effect);
    const std::size_t down_line = buffer.position_of(down_effect.selection.caret).line.value;
    expect(down_effect.first_visible == LineNumber{16},
           "a huge Ctrl-d clamps its amount to the viewport height");
    expect(down_line >= down_effect.first_visible.value &&
               down_line < down_effect.first_visible.value +
                               nenenib::core::vim_viewport_of(view).visible_lines,
           "Ctrl-d returns a caret inside the viewport it returns");
    expect(down.next.scroll_lines.value_or(nenenib::core::VimCount{0}).value == 10,
           "Ctrl-d stores the clamped amount");
    const auto up = vim_step(state, view, VimKey{VimSpecialKey::control_u});
    const auto &up_effect = std::get<VimNavigate>(up.effect);
    const std::size_t up_line = buffer.position_of(up_effect.selection.caret).line.value;
    expect(up_effect.first_visible == LineNumber{1}, "a huge Ctrl-u stops at the first line");
    expect(up_line >= up_effect.first_visible.value &&
               up_line < up_effect.first_visible.value +
                             nenenib::core::vim_viewport_of(view).visible_lines,
           "Ctrl-u returns a caret inside the viewport it returns");
}

void verify_vim_viewport_page_edges()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport sample parses");
    const auto &buffer = text.value();
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    state.count = nenenib::core::VimCount{999};
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{6}, 10, 0}};
    const auto page_down = vim_step(state, view, VimKey{VimSpecialKey::control_f});
    const auto &page_down_effect = std::get<VimNavigate>(page_down.effect);
    expect(page_down_effect.first_visible == LineNumber{30} &&
               buffer.position_of(page_down_effect.selection.caret).line == LineNumber{30},
           "a huge Ctrl-f reaches the last line without looping");
    const VimEditorView lower_view{buffer, collapsed_at(buffer.line_start(LineNumber{25})),
                                   VimViewport{LineNumber{21}, 10, 0}};
    const auto page_up = vim_step(state, lower_view, VimKey{VimSpecialKey::control_b});
    const auto &page_up_effect = std::get<VimNavigate>(page_up.effect);
    expect(page_up_effect.first_visible == LineNumber{1} &&
               buffer.position_of(page_up_effect.selection.caret).line == LineNumber{6},
           "a huge Ctrl-b keeps the overlap of its last effective page");
    const VimEditorView top_view{buffer, collapsed_at(buffer.line_start(LineNumber{10})),
                                 VimViewport{LineNumber{1}, 10, 0}};
    const auto page_at_top = vim_step(state, top_view, VimKey{VimSpecialKey::control_b});
    const auto &page_at_top_effect = std::get<VimNavigate>(page_at_top.effect);
    expect(page_at_top_effect.first_visible == LineNumber{1} &&
               page_at_top_effect.selection == top_view.selection,
           "Ctrl-b at the top preserves the caret exactly");
}

void verify_vim_viewport_state_lifetime()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply_frame(VisibleLines{10}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply_frame(
        PlaceCaret{TextPosition{LineNumber{10}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply_frame(ScrollLines{5}));
    vim_replay(controller, "3<C-d>j<C-d><Esc><C-u>");
    expect(controller.vim_state().scroll_lines.value_or(nenenib::core::VimCount{0}).value == 3,
           "ordinary Vim commands preserve an explicit half-page amount");
    static_cast<void>(controller.apply_frame(VisibleLines{10}));
    expect(controller.vim_state().scroll_lines.has_value(),
           "a repeated notification of the same height preserves it");

    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    expect(controller.vim_state().scroll_lines.has_value(), "opening another file preserves it");
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    expect(controller.vim_state().scroll_lines.has_value(), "mode toggles preserve it");

    static_cast<void>(controller.apply_frame(ScrollLines{100}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::ordinary}));
    expect(controller.frame().first_visible == LineNumber{21},
           "ordinary mode restores the filled-viewport scroll bound");
    static_cast<void>(controller.apply_frame(VisibleLines{11}));
    expect(!controller.vim_state().scroll_lines.has_value(),
           "a real height change resets the explicit half-page amount");
}

void verify_vim_visual_scroll_recovers_viewport()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply_frame(VisibleLines{10}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply_frame(
        PlaceCaret{TextPosition{LineNumber{10}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply_frame(ScrollLines{5}));
    vim_replay(controller, "v");
    static_cast<void>(controller.apply_frame(ScrollLines{15}));
    const auto frame = controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::control_d}});
    expect(frame.vim_mode == VimMode::visual && frame.first_visible == LineNumber{15} &&
               frame.caret.position.line == LineNumber{15},
           "VISUAL Ctrl-d brings a wheel-hidden caret into its returned viewport");
}

void verify_vim_follow_at_document_end()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply_frame(VisibleLines{10}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply_frame(
        PlaceCaret{TextPosition{LineNumber{15}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply_frame(ScrollLines{5}));
    vim_replay(controller, "99j");
    expect(controller.frame().caret.position.line == LineNumber{30} &&
               controller.frame().first_visible == LineNumber{21},
           "an automatic jump fills the last viewport");

    static_cast<void>(controller.apply_frame(
        PlaceCaret{TextPosition{LineNumber{30}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply_frame(ScrollLines{9}));
    vim_replay(controller, "<C-f>k");
    expect(controller.frame().caret.position.line == LineNumber{29} &&
               controller.frame().first_visible == LineNumber{29},
           "a motion after an explicit EOF page preserves its trailing blank area");
}

// Vim モードではクリックと Ctrl+矢印のあとも文字の上へ寄る（Vim も行末より右のクリックは
// 最後の文字に置く）。INSERT は行末の右に居てよいので寄せない。
void verify_vim_caret_placement()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("alpha\nbeta")});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply_frame(
        PlaceCaret{TextPosition{LineNumber{1}, Column{99}}, SelectionAnchoring::extend}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{5}},
           "a click past the line end lands on the last character");
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "Shift does not open a selection in Vim mode");
    static_cast<void>(
        controller.apply_frame(MoveCaret{CaretMotion::document_end, SelectionAnchoring::extend}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{4}},
           "Ctrl+End settles onto the last character too");
    vim_replay(controller, "i");
    static_cast<void>(controller.apply_frame(
        PlaceCaret{TextPosition{LineNumber{2}, Column{99}}, SelectionAnchoring::collapse}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{5}},
           "INSERT may sit past the last character");
}
} // namespace

// fixture を 1 件再生する。本文・キャレット・無名レジスタ（本文と種類）を本物の Vim
// と突き合わせる。
void verify_vim_fixture(const VimFixture &fixture)
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(fixture.text)});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    arrange_vim_viewport(controller, fixture);
    store_vim_fixture_macro(controller, fixture);
    // oracle は鍵を 1 本の :normal! で流すので、鍵が失敗したら残りを流さない（Issue #230）。
    vim_normal(controller, fixture.keys);
    const auto frame = controller.frame();
    const std::string name(fixture.name);
    const std::size_t expected_first =
        fixture.viewport.has_value() ? fixture.viewport.value().expected_first_visible : 1;
    expect(frame.first_visible == LineNumber{expected_first}, (name + ": viewport").c_str());
    expect(frame.caret.position.line.value == static_cast<std::size_t>(fixture.line),
           (name + ": line").c_str());
    expect(vim_byte_column(std::string(fixture.expected_text), frame.caret.position) ==
               static_cast<std::size_t>(fixture.column),
           (name + ": column").c_str());
    expect(controller.vim_state().unnamed_register.value().text == fixture.register_text,
           (name + ": register").c_str());
    expect(vim_register_kind(controller.vim_state().unnamed_register.value()) ==
               fixture.register_kind,
           (name + ": register kind").c_str());
    if (fixture.viewport.has_value())
    {
        const std::size_t fallback =
            std::max<std::size_t>(fixture.viewport.value().visible_lines / 2, 1);
        const std::size_t scroll_lines =
            controller.vim_state().scroll_lines.value_or(nenenib::core::VimCount{fallback}).value;
        expect(scroll_lines == fixture.viewport.value().expected_scroll_lines,
               (name + ": scroll lines").c_str());
        if (fixture.viewport.value().wrap_columns > 0)
        {
            expect(frame.lines.front().screen_row ==
                       -static_cast<std::int64_t>(fixture.viewport.value().expected_first_row),
                   (name + ": first wrapped row").c_str());
        }
        // 折り返さない窓の leftcol（1 升 10 px・ADR 0145）。
        if (fixture.viewport.value().nowrap_columns > 0)
        {
            expect(frame.horizontal ==
                       static_cast<float>(fixture.viewport.value().expected_left_column * 10),
                   (name + ": left column").c_str());
        }
    }
    const std::string body = whole_vim_body(controller);
    expect(body == fixture.expected_text, (name + ": body").c_str());
}

// INSERT にいるあいだの編集は 1 つの undo 単位（ADR 0015 の決定 5）。oracle は :normal! 1 回を
// まるごと 1 単位にする（"xxu" が 2 つの x を両方戻すことを実測）ので、単位はここで手で測る。
void verify_vim_insert_undo_unit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello")});
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "ia<BS>b<Esc>");
    expect(vim_body(controller.frame()) == "bhello", "the backspace ate the character before it");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello",
           "one undo takes back the whole insert, deletion and all");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "there is nothing before that unit");
}

// c の削除は INSERT の単位に入る（cw のあとに打った文字と 1 つの Edit になる）。
void verify_vim_change_undo_unit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello world")});
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "cwbye<Esc>");
    expect(vim_body(controller.frame()) == "bye world", "cw changed the first word");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello world",
           "one undo takes back both the deletion and the typing");
}

// INSERT の中で動くと単位は切れる（ADR 0015 の決定 5）。本物の Vim は挿入 1 回を丸ごと
// 1 単位にするので、ここは Vim と違う＝ oracle には載せられない（報告と 5-g に書く）。
void verify_vim_insert_motion_breaks_the_unit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello")});
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "ia");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::arrow_left}}));
    vim_replay(controller, "b<Esc>");
    expect(vim_body(controller.frame()) == "bahello", "the arrow moved back before the insert");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "ahello", "the arrow broke the unit");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "the second undo takes back the first letter");
}

// CRLF の文書でもレジスタは LF（決定 3）。文書の改行に戻すのは controller の 1 か所（ARC-009）。
void verify_vim_put_line_endings()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\r\ntwo\r\nthree")});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "dd");
    expect(controller.vim_state().unnamed_register.value().text == "one\n",
           "the register holds LF only");
    expect(controller.vim_state().unnamed_register.value().kind == VimRegisterKind::lines,
           "and it knows it is a line");
    vim_replay(controller, "p");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "two\r\none\r\nthree", "the put line came back as CRLF");
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "the caret sits on the line that was put");
    vim_replay(controller, "jp");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "two\r\none\r\nthree\r\none",
           "putting after the last line brings the CRLF with it");
    expect(controller.frame().caret.position == TextPosition{LineNumber{4}, Column{1}},
           "and the caret counts the CR that the engine did not see");
}

void verify_vim_crlf()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\r\ntwo\r\nthree")});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "$");
    expect(controller.frame().caret.position.column == Column{3},
           "$ stops on the last character, not on the CR");
    vim_replay(controller, "x");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "on\r\ntwo\r\nthree", "x left the CRLF alone");
    vim_replay(controller, "jdd");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "on\r\nthree", "dd took the whole CRLF with the line");
    vim_replay(controller, "jdd");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "on", "dd on the last line took the CRLF before it");
    verify_vim_crlf_documents();
}

void verify_vim_step_edges()
{
    const auto text = TextBuffer::from_utf8("ab");
    expect(text.has_value(), "the two character buffer parses");
    const auto &buffer = text.value();
    VimState inserting =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    inserting.mode = VimMode::insert;
    const auto newline =
        vim_step(inserting,
                 VimEditorView{buffer, collapsed_at(Offset{1}), VimViewport{LineNumber{1}, 64, 0}},
                 VimKey{VimCharacter{U'\n'}});
    expect(std::holds_alternative<VimNewLine>(newline.effect),
           "a newline typed as a character becomes the buffer's own line ending");
    const auto at_start =
        vim_step(inserting,
                 VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64, 0}},
                 VimKey{VimSpecialKey::backspace});
    expect(std::holds_alternative<VimNoEffect>(at_start.effect),
           "Backspace at the start of the buffer does nothing");
    const VimState resting =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    const auto unbound = vim_step(
        resting, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64, 0}},
        VimKey{VimCharacter{U'z'}});
    expect(std::holds_alternative<VimNoEffect>(unbound.effect), "an unbound key does nothing");
    expect(unbound.next.mode == VimMode::normal, "and it leaves NORMAL alone");
}

void verify_vim_viewport_mode_edges()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport sample parses");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{6}, 10, 0}};
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    VimState remembered = state;
    remembered.count = std::nullopt;
    remembered.scroll_lines = nenenib::core::VimCount{3};
    expect(nenenib::core::vim_after_resize(remembered, 10, 10).scroll_lines.has_value(),
           "an unchanged height preserves the half-page amount");
    expect(!nenenib::core::vim_after_resize(remembered, 10, 11).scroll_lines.has_value(),
           "a changed height resets the half-page amount");
    expect(nenenib::core::vim_resting_from(remembered, remembered.unnamed_register.value())
               .scroll_lines.has_value(),
           "resting transitions preserve the half-page amount");

    VimState inserting = remembered;
    inserting.mode = VimMode::insert;
    for (const VimSpecialKey key : {VimSpecialKey::control_d, VimSpecialKey::control_u,
                                    VimSpecialKey::control_f, VimSpecialKey::control_b})
    {
        const auto ignored = vim_step(inserting, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(ignored.effect),
               "INSERT ignores Ctrl viewport keys");
    }

    VimState visual = remembered;
    visual.mode = VimMode::visual;
    const std::array<std::pair<char32_t, LineNumber>, 3> screen_lines{
        {{U'H', LineNumber{6}}, {U'M', LineNumber{10}}, {U'L', LineNumber{15}}}};
    for (const auto &[key, expected] : screen_lines)
    {
        const auto moved = vim_step(visual, view, VimKey{VimCharacter{key}});
        const auto &effect = std::get<VimSelect>(moved.effect);
        expect(effect.selection.anchor == selection.anchor &&
                   buffer.position_of(effect.selection.caret).line == expected,
               "VISUAL H M L preserve the anchor and move on screen lines");
    }
    const auto visual_forward = vim_step(visual, view, VimKey{VimSpecialKey::control_f});
    const auto &visual_forward_effect = std::get<VimNavigate>(visual_forward.effect);
    expect(visual_forward_effect.selection.anchor == selection.anchor &&
               visual_forward_effect.first_visible == LineNumber{14} &&
               buffer.position_of(visual_forward_effect.selection.caret).line == LineNumber{14},
           "VISUAL Ctrl-f preserves the anchor and uses the NORMAL viewport transition");
    const auto visual_back = vim_step(visual, view, VimKey{VimSpecialKey::control_b});
    const auto &visual_back_effect = std::get<VimNavigate>(visual_back.effect);
    expect(visual_back_effect.selection.anchor == selection.anchor &&
               visual_back_effect.first_visible == LineNumber{1} &&
               buffer.position_of(visual_back_effect.selection.caret).line == LineNumber{7},
           "VISUAL Ctrl-b preserves the anchor and uses the NORMAL viewport transition");
}

void verify_vim_insert_page_move_breaks_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("x\nx")});
    static_cast<void>(controller.apply_frame(VisibleLines{2}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "ia<PageDown>b<Esc>u");
    expect(whole_vim_body(controller) == "ax\nx",
           "undo after INSERT PageDown takes back only the second insertion");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "x\nx",
           "the earlier insertion remains a separate undo unit");
}

// VISUAL の入口は選択そのもの（ADR 0018 の決定 2）。NORMAL / INSERT は anchor を読まない。
void verify_vim_visual_step_edges()
{
    const auto text = TextBuffer::from_utf8("abc\ndef");
    expect(text.has_value(), "the two line buffer parses");
    const auto &buffer = text.value();
    const VimState resting =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    const auto entered = vim_step(
        resting, VimEditorView{buffer, collapsed_at(Offset{1}), VimViewport{LineNumber{1}, 64, 0}},
        VimKey{VimCharacter{U'v'}});
    expect(entered.next.mode == VimMode::visual, "v enters VISUAL");
    expect(std::get<VimSelect>(entered.effect).selection == Selection{Offset{1}, Offset{1}},
           "and anchors the selection where the caret is");
    VimState visual = resting;
    visual.mode = VimMode::visual;
    const Selection selection{Offset{1}, Offset{2}};
    // VISUAL で効かない鍵は選択もモードも動かさない（決定 7）。`<CR>` は ADR 0048 の決定 9 で
    // `+` と同じ移動に、`<BS>` は ADR 0049 で行をまたぐ h になったので、ここには効かない文字
    // `Z` `Q` を置く。
    for (const VimKey &key :
         {VimKey{VimCharacter{U'p'}}, VimKey{VimCharacter{U'u'}}, VimKey{VimCharacter{U'D'}},
          VimKey{VimCharacter{U'A'}}, VimKey{VimCharacter{U'Z'}}, VimKey{VimCharacter{U'Q'}},
          VimKey{VimSpecialKey::control_r}, VimKey{VimCharacter{U'z'}}})
    {
        const auto step = vim_step(
            visual, VimEditorView{buffer, selection, VimViewport{LineNumber{1}, 64, 0}}, key);
        expect(std::holds_alternative<VimNoEffect>(step.effect),
               "a key outside this slice does nothing in VISUAL");
        expect(step.next.mode == VimMode::visual, "and stays in VISUAL");
    }
    const auto escaped =
        vim_step(visual, VimEditorView{buffer, selection, VimViewport{LineNumber{1}, 64, 0}},
                 VimKey{VimSpecialKey::escape});
    expect(std::get<VimMoveTo>(escaped.effect).caret == Offset{2}, "Esc leaves the caret alone");
    expect(escaped.next.mode == VimMode::normal, "and goes back to NORMAL");
    // 表示の範囲と操作の範囲は同じ 1 本（決定 5 / ADR 0018 の強制）。
    const auto characters = vim_visual_range(buffer, selection, VimMode::visual);
    expect(characters.range == OffsetRange{Offset{1}, Offset{3}} &&
               characters.kind == VimRegisterKind::characters,
           "the charwise range takes the character under the far end");
    const auto lines = vim_visual_range(buffer, selection, VimMode::visual_line);
    expect(lines.range == OffsetRange{Offset{0}, Offset{3}} && lines.kind == VimRegisterKind::lines,
           "the linewise range takes whole lines");
    const auto across =
        vim_visual_range(buffer, Selection{Offset{1}, Offset{5}}, VimMode::visual_line);
    expect(across.range == OffsetRange{Offset{0}, Offset{7}}, "and spans both lines");
    const auto none = vim_visual_range(buffer, collapsed_at(Offset{2}), VimMode::normal);
    expect(is_empty(none.range), "NORMAL has no visual range");
    const auto inserting_none = vim_visual_range(buffer, collapsed_at(Offset{2}), VimMode::insert);
    expect(is_empty(inserting_none.range), "and neither has INSERT");
}

namespace
{
// `gt` `gT` の engine（ADR 0057 の決定 2）。鍵を順に流し、最後の 1 鍵の結果を返す。本文と選択と
// タブの値は鍵のあいだで変えない（engine が借用だけすることを見る）。
[[nodiscard]] nenenib::core::VimStep tab_step(std::string_view keys, nenenib::core::VimTabs tabs)
{
    const auto text = TextBuffer::from_utf8("abc def");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(Offset{4});
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, 64, 0},
                             nenenib::core::VimKeySource::typed, tabs};
    nenenib::core::VimStep last{
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters}),
        VimNoEffect{}};
    for (const VimKey &key : vim_keys_of(keys))
    {
        last = vim_step(last.next, view, key);
    }
    return last;
}

[[nodiscard]] bool switched_to(const nenenib::core::VimStep &step, std::size_t index)
{
    const auto *const effect = std::get_if<nenenib::core::VimSwitchTab>(&step.effect);
    return effect != nullptr && effect->index == index && !step.failure.has_value() &&
           step.next.mode == VimMode::normal && !step.next.count.has_value() &&
           !step.next.input_wait.has_value();
}

[[nodiscard]] bool refused_tab(const nenenib::core::VimStep &step, VimMode mode)
{
    return std::holds_alternative<VimNoEffect>(step.effect) && step.failure.has_value() &&
           step.next.mode == mode && !step.next.count.has_value() &&
           !step.next.pending.has_value() && !step.next.input_wait.has_value();
}

void verify_vim_tab_steps()
{
    using nenenib::core::VimTabs;
    const VimTabs three{1, 3};
    expect(switched_to(tab_step("gt", three), 2) && switched_to(tab_step("gT", three), 0),
           "gt and gT name the neighbouring tab");
    expect(switched_to(tab_step("gt", VimTabs{2, 3}), 0) &&
               switched_to(tab_step("gT", VimTabs{0, 3}), 2),
           "gt and gT wrap around the band");
    expect(switched_to(tab_step("3gt", three), 2) && switched_to(tab_step("1gt", three), 0) &&
               switched_to(tab_step("2gt", three), 1),
           "a count before gt is the tab number, the current tab included");
    expect(switched_to(tab_step("2gT", three), 2) && switched_to(tab_step("9gT", three), 1),
           "a count before gT goes back that many tabs and wraps");
    const auto outside = tab_step("4gt", three);
    expect(refused_tab(outside, VimMode::normal) &&
               outside.failure == std::optional{nenenib::core::VimRepeatFailure::not_moved},
           "a tab number past the band fails without an effect");
    expect(switched_to(tab_step("gt", VimTabs{0, 1}), 0) &&
               switched_to(tab_step("3gT", VimTabs{0, 1}), 0) &&
               refused_tab(tab_step("2gt", VimTabs{0, 1}), VimMode::normal),
           "with one tab gt and gT stay and 2gt fails");
    for (const std::string_view keys : {"dgt", "ygt", "cgt", "d2gt", "dgT"})
    {
        expect(refused_tab(tab_step(keys, three), VimMode::normal),
               "an operator before gt is cancelled with a failure");
    }
    expect(switched_to(tab_step("vgt", three), 2), "gt in VISUAL switches and rests in NORMAL");
    expect(refused_tab(tab_step("v9gt", three), VimMode::visual),
           "a failing gt in VISUAL keeps VISUAL");
    const auto after_change = tab_step("xgt", three);
    expect(after_change.next.last_change.has_value() &&
               after_change.next.last_change.value().keys.owned_keys() == vim_keys_of("x"),
           "gt keeps the last change for the dot");
}

[[nodiscard]] bool same_vim_effect(const nenenib::core::VimEffect &left,
                                   const nenenib::core::VimEffect &right)
{
    if (left.index() != right.index())
    {
        return false;
    }
    if (const auto *navigate = std::get_if<VimNavigate>(&left))
    {
        const auto &other = std::get<VimNavigate>(right);
        return navigate->selection == other.selection &&
               navigate->first_visible == other.first_visible;
    }
    if (const auto *moved = std::get_if<VimMoveTo>(&left))
    {
        return moved->caret == std::get<VimMoveTo>(right).caret;
    }
    if (const auto *selected = std::get_if<VimSelect>(&left))
    {
        return selected->selection == std::get<VimSelect>(right).selection;
    }
    if (const auto *removed = std::get_if<nenenib::core::VimRemoveLines>(&left))
    {
        return removed->range == std::get<nenenib::core::VimRemoveLines>(right).range;
    }
    return true;
}

[[nodiscard]] bool same_vim_step(const nenenib::core::VimStep &left,
                                 const nenenib::core::VimStep &right)
{
    return same_vim_effect(left.effect, right.effect) && left.next.mode == right.next.mode &&
           left.next.count == right.next.count &&
           left.next.pending.has_value() == right.next.pending.has_value() &&
           left.next.scroll_lines == right.next.scroll_lines &&
           left.failure.has_value() == right.failure.has_value() &&
           left.viewport_request == right.viewport_request;
}

[[nodiscard]] VimState vim_state_after(std::string_view keys, const VimEditorView &view)
{
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    for (const VimKey &key : vim_keys_of(keys))
    {
        state = vim_step(state, view, key).next;
    }
    return state;
}

// 両方に値があるときだけ中身を比べる。片方だけなら違う。
template <typename Value, typename Same>
[[nodiscard]] bool same_optional(const std::optional<Value> &left,
                                 const std::optional<Value> &right, Same same)
{
    if (!left.has_value() || !right.has_value())
    {
        return left.has_value() == right.has_value();
    }
    return same(left.value(), right.value());
}

[[nodiscard]] bool same_vim_register(const VimRegisterSnapshot &left,
                                     const VimRegisterSnapshot &right)
{
    const VimRegister &before = left.value();
    const VimRegister &after = right.value();
    return before.text == after.text && before.kind == after.kind && before.width == after.width;
}

[[nodiscard]] bool same_vim_registers(const VimState &left, const VimState &right)
{
    return same_vim_register(left.unnamed_register, right.unnamed_register) &&
           same_vim_register(left.small_delete, right.small_delete) &&
           same_vim_register(left.clipboard, right.clipboard) &&
           std::ranges::equal(left.registers.registers, right.registers.registers,
                              same_vim_register) &&
           std::ranges::equal(left.numbered.registers, right.numbered.registers, same_vim_register);
}

[[nodiscard]] bool same_vim_record(const std::optional<VimRepeatRecord> &left,
                                   const std::optional<VimRepeatRecord> &right)
{
    return same_optional(left, right,
                         [](const VimRepeatRecord &before, const VimRepeatRecord &after)
                         {
                             return before.count == after.count &&
                                    before.visual.has_value() == after.visual.has_value() &&
                                    before.keys.owned_keys() == after.keys.owned_keys();
                         });
}

[[nodiscard]] bool same_vim_waits(const VimState &left, const VimState &right)
{
    return same_optional(
               left.pending, right.pending,
               [](const VimPendingOperator &before, const VimPendingOperator &after)
               { return before.operation == after.operation && before.count == after.count; }) &&
           same_optional(left.selected_register, right.selected_register,
                         [](const VimRegisterSelection &before, const VimRegisterSelection &after)
                         {
                             return before.target == after.target && before.name == after.name &&
                                    before.append == after.append;
                         }) &&
           same_optional(
               left.insert_repeat, right.insert_repeat,
               [](const VimInsertRepeat &before, const VimInsertRepeat &after)
               { return before.remaining == after.remaining && before.text == after.text; }) &&
           left.input_wait.has_value() == right.input_wait.has_value();
}

// 表示領域や文字組みを求めた step が何も消費しないこと。回数・オペレータ・レジスタ・録画・`.` の
// 記録・欲しい列を比べる（VimState は == を持たないので欄ごと）。
[[nodiscard]] bool same_vim_memory(const VimState &left, const VimState &right)
{
    return left.mode == right.mode && left.count == right.count &&
           left.wanted_column == right.wanted_column && left.scroll_lines == right.scroll_lines &&
           left.last_macro == right.last_macro && same_vim_waits(left, right) &&
           same_vim_record(left.recording, right.recording) &&
           same_vim_record(left.last_change, right.last_change) &&
           same_optional(left.macro_recording, right.macro_recording,
                         [](const VimMacroRecording &before, const VimMacroRecording &after)
                         {
                             return before.name == after.name && before.append == after.append &&
                                    before.keys.owned_keys() == after.keys.owned_keys();
                         }) &&
           same_vim_registers(left, right);
}

// 画面を読む鍵は表示領域が無ければ要求だけを立てて状態を動かさず、渡して流し直すと最初から
// 渡したのと同じ結果になる（ADR 0129 の決定 1・3）。録画・レジスタ・`.` の記録も消費しない。
void verify_vim_viewport_reading_keys(const VimEditorView &shown, const VimEditorView &hidden)
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 26> reading{{
        {"", "H"},          {"", "M"},        {"", "L"},           {"3", "H"},
        {"2", "L"},         {"d", "L"},       {"y2", "H"},         {"v", "H"},
        {"", "<C-d>"},      {"5", "<C-u>"},   {"", "<C-f>"},       {"", "<C-b>"},
        {"", "<PageDown>"}, {"", "<PageUp>"}, {"i", "<PageDown>"}, {"i", "<PageUp>"},
        {"v", "<C-d>"},     {"V", "H"},       {"V", "M"},          {"V", "L"},
        {"<C-v>", "H"},     {"<C-v>", "M"},   {"<C-v>", "L"},      {"xqa", "H"},
        {"\"ayyqb", "M"},   {"\"b", "L"},
    }};
    for (const auto &[before, key] : reading)
    {
        const VimState state = vim_state_after(before, shown);
        const VimKey last = vim_keys_of(key).front();
        const auto requested = vim_step(state, hidden, last);
        expect(requested.viewport_request && !requested.layout_request.has_value() &&
                   std::holds_alternative<VimNoEffect>(requested.effect),
               "a key that reads the screen requests the viewport without an effect");
        expect(same_vim_memory(requested.next, state),
               "the viewport request consumes no count, operator, register, recording or record");
        expect(same_vim_step(vim_step(requested.next, shown, last), vim_step(state, shown, last)),
               "the answered key matches a step that had the viewport from the start");
    }
}

// 読まない鍵は要求しない（ADR 0129 の決定 1）。
void verify_vim_viewport_blind_keys(const VimEditorView &shown, const VimEditorView &hidden)
{
    constexpr std::array<std::pair<std::string_view, std::string_view>, 11> blind{{
        {"", "h"},
        {"", "j"},
        {"", "k"},
        {"", "l"},
        {"", "w"},
        {"", "b"},
        {"", "x"},
        {"", "i"},
        {"i", "a"},
        {"i", "<Esc>"},
        {"d", "j"},
    }};
    for (const auto &[before, key] : blind)
    {
        const VimState state = vim_state_after(before, shown);
        const VimKey last = vim_keys_of(key).front();
        const auto stepped = vim_step(state, hidden, last);
        expect(!stepped.viewport_request && same_vim_step(stepped, vim_step(state, shown, last)),
               "a key that does not read the screen steps without the viewport");
    }
}

// 折り返しの試料の行の画面行数。行 n は 1 + n % 3 行。
[[nodiscard]] std::vector<VimLineRows> wrapped_rows_of(std::size_t lines)
{
    std::vector<VimLineRows> rows;
    for (std::size_t line = 1; line <= lines; ++line)
    {
        rows.push_back(VimLineRows{LineNumber{line}, 1 + (line % 3)});
    }
    return rows;
}

constexpr VimViewport wrapped_viewport{LineNumber{7}, 10, 1};

// controller と同じ順で 1 つの要求に応える。文字組みの要求は viewport を渡した後にだけ来る。
void answer_wrapped_request(const nenenib::core::VimStep &step, VimEditorView &view,
                            std::vector<VimLineRows> &rows, std::span<const VimLineRows> all)
{
    expect(!(step.viewport_request && step.layout_request.has_value()),
           "a step asks for one thing at a time");
    if (step.viewport_request)
    {
        expect(!view.viewport.has_value(), "the viewport is requested once");
        view.viewport = wrapped_viewport;
        return;
    }
    expect(view.viewport.has_value(), "the layout request follows the viewport");
    const LineNumber line = step.layout_request.value_or(LineNumber{1});
    const auto found = std::ranges::lower_bound(rows, line.value, {}, [](const VimLineRows &value)
                                                { return value.line.value; });
    rows.insert(found, all[line.value - 1]);
    view.layout.lines = rows;
}

// 表示領域も行の画面行数も無い所から、要求に順に応えて同じ鍵を流し直した結果。
[[nodiscard]] nenenib::core::VimStep answered_wrapped(const VimState &state,
                                                      const VimEditorView &hidden,
                                                      const VimKey &key,
                                                      std::span<const VimLineRows> all)
{
    std::vector<VimLineRows> rows;
    VimEditorView view = hidden;
    auto step = vim_step(state, view, key);
    expect(step.viewport_request,
           "a wrapped key that reads the screen asks for the viewport first");
    for (std::size_t round = 0;
         round <= all.size() && (step.viewport_request || step.layout_request.has_value()); ++round)
    {
        expect(same_vim_memory(step.next, state), "a wrapped request consumes nothing");
        answer_wrapped_request(step, view, rows, all);
        step = vim_step(state, view, key);
    }
    return step;
}

// 折り返しが有効でも、viewport の要求 → 行の画面行数の要求の往復に応えると、最初から全部を
// 渡したのと同じ結果になる（ADR 0129 の決定 1・2）。
void verify_vim_wrapped_viewport_request()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the wrapped viewport request sample parses");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const auto all = wrapped_rows_of(30);
    const VimEditorView full{buffer,           selection,
                             wrapped_viewport, VimKeySource::typed,
                             VimTabs{0, 1},    VimLayoutView{true, all}};
    const VimEditorView hidden{buffer,        selection,
                               std::nullopt,  VimKeySource::typed,
                               VimTabs{0, 1}, VimLayoutView{true, {}}};
    constexpr std::array<std::pair<std::string_view, std::string_view>, 7> keys{{
        {"", "H"},
        {"", "L"},
        {"v", "M"},
        {"V", "L"},
        {"", "<C-d>"},
        {"", "<C-f>"},
        {"", "<PageUp>"},
    }};
    for (const auto &[before, key] : keys)
    {
        const VimState state = vim_state_after(before, full);
        const VimKey last = vim_keys_of(key).front();
        const auto answered = answered_wrapped(state, hidden, last, all);
        expect(!answered.viewport_request && !answered.layout_request.has_value() &&
                   same_vim_step(answered, vim_step(state, full, last)),
               "the answered wrapped key matches a step that had everything from the start");
    }
}

// 画面行の単位（ADR 0144 の決定 2・5）。8 字ごとに折り返す 1 行 40 字の行を、controller と同じ順で
// 問いに答えて流す。line の単位は同じ鍵で画面行を問わず、1 行の文書では動かない（今日のまま）。
[[nodiscard]] nenenib::core::VimRowSpan row_span_of(const TextBuffer &text,
                                                    const nenenib::core::VimRowRequest &request)
{
    constexpr std::size_t width = 8;
    const std::size_t length = text.line_end(request.line).value;
    const std::size_t rows = (length + width - 1) / width;
    const auto *const at = std::get_if<nenenib::core::Offset>(&request.target);
    const std::size_t row = at != nullptr ? std::min(at->value / width, rows - 1)
                                          : std::get<std::size_t>(request.target);
    return nenenib::core::VimRowSpan{request.line,
                                     row,
                                     nenenib::core::Offset{row * width},
                                     nenenib::core::Offset{std::min((row + 1) * width, length)},
                                     rows,
                                     row * width};
}

[[nodiscard]] nenenib::core::VimStep rows_answered(const VimState &state, const TextBuffer &text,
                                                   nenenib::core::VimScrollUnit unit,
                                                   const VimKey &key)
{
    const Selection selection = collapsed_at(nenenib::core::Offset{2});
    std::vector<VimLineRows> rows;
    std::vector<nenenib::core::VimRowSpan> spans;
    VimEditorView view{text,          selection,
                       std::nullopt,  VimKeySource::typed,
                       VimTabs{0, 1}, VimLayoutView{true, rows, unit, spans}};
    auto step = vim_step(state, view, key);
    for (std::size_t round = 0; round < 16; ++round)
    {
        if (step.viewport_request)
        {
            view.viewport = VimViewport{LineNumber{1}, 3, 0};
        }
        else if (step.layout_request.has_value())
        {
            rows.push_back(VimLineRows{step.layout_request.value(), 5});
            view.layout.lines = rows;
        }
        else if (step.row_request.has_value())
        {
            expect(unit == nenenib::core::VimScrollUnit::row, "only the row unit asks for rows");
            spans.push_back(row_span_of(text, step.row_request.value()));
            view.layout.spans = spans;
        }
        else
        {
            break;
        }
        step = vim_step(state, view, key);
    }
    return step;
}

// 索引を持つ本文の桁は、行を写して数えた桁と同じ（ADR 0144・VirtualColumn の 1 本）。
void verify_indexed_virtual_columns()
{
    const auto plain = TextBuffer::from_utf8("a\t\xE6\x97\xA5"
                                             "e\xCC\x81\x01z\n\tb\xE5\x85\xA8\t\nc");
    expect(plain.has_value(), "the indexed column sample parses");
    const TextBuffer indexed = plain.value().with_virtual_columns();
    const auto all = plain.value().text_range(nenenib::core::Offset{0},
                                              nenenib::core::Offset{plain.value().size_bytes()});
    bool same = true;
    for (nenenib::core::Offset at{0}; at.value <= all.size();
         at = at.value < all.size() ? nenenib::core::next_code_point(all, at)
                                    : nenenib::core::Offset{all.size() + 1})
    {
        same = same &&
               nenenib::core::caret_virtual_column(indexed, at) ==
                   nenenib::core::caret_virtual_column(plain.value(), at) &&
               nenenib::core::virtual_column(indexed, at) ==
                   nenenib::core::virtual_column(plain.value(), at);
    }
    expect(same, "an indexed text answers the same virtual columns as a copied line");
}

// 塊（約 64 KiB）の境をまたぐ本文でも、索引の桁は行を写して数えた桁と同じ（I-5）。Tab・全角・
// 結合文字・制御文字の並びが塊の境で割れる。境の前後と一定の間隔の位置で照合する。
void verify_indexed_columns_across_chunks()
{
    std::string line;
    while (line.size() < 200'000)
    {
        line += "ab\t\xE6\x97\xA5"
                "e\xCC\x81\x01z\t";
    }
    const auto plain = TextBuffer::from_utf8(line + "\n" + line);
    expect(plain.has_value(), "the chunked column sample parses");
    const TextBuffer indexed = plain.value().with_virtual_columns();
    const auto chunks = indexed.virtual_columns().value().get().chunks();
    expect(chunks.size() >= 4, "the sample spans several chunks");
    const auto all = plain.value().text_range(nenenib::core::Offset{0},
                                              nenenib::core::Offset{plain.value().size_bytes()});
    const auto near_boundary = [&chunks](std::size_t at)
    {
        return std::ranges::any_of(chunks, [at](const auto &chunk)
                                   { return at + 64 >= chunk.start && at <= chunk.start + 64; });
    };
    bool same = true;
    std::size_t checked = 0;
    for (nenenib::core::Offset at{0}; at.value < all.size();
         at = nenenib::core::next_code_point(all, at))
    {
        if (!near_boundary(at.value) && at.value % 4'099 != 0)
        {
            continue;
        }
        ++checked;
        same = same &&
               nenenib::core::caret_virtual_column(indexed, at) ==
                   nenenib::core::caret_virtual_column(plain.value(), at) &&
               nenenib::core::virtual_column(indexed, at) ==
                   nenenib::core::virtual_column(plain.value(), at);
    }
    expect(same && checked > 100,
           "an indexed text answers the same columns as a copied line across chunk boundaries");
}

void verify_vim_row_unit_pages()
{
    const auto text = TextBuffer::from_utf8(std::string(40, 'a'));
    expect(text.has_value(), "the row unit sample parses");
    const Selection origin = collapsed_at(nenenib::core::Offset{2});
    const VimState state = vim_state_after("", VimEditorView{text.value(), origin, std::nullopt});
    const VimKey page = vim_keys_of("<PageDown>").front();
    const auto rowed = rows_answered(state, text.value(), nenenib::core::VimScrollUnit::row, page);
    const auto *const down = std::get_if<VimNavigate>(&rowed.effect);
    expect(down != nullptr && down->first_visible == LineNumber{1} && down->first_row == 3 &&
               down->selection.caret == nenenib::core::Offset{26},
           "the row unit PgDn moves the top and the caret 3 screen rows on the same column");
    const auto lined = rows_answered(state, text.value(), nenenib::core::VimScrollUnit::line, page);
    const auto *const stayed = std::get_if<VimNavigate>(&lined.effect);
    expect(stayed != nullptr && stayed->first_row == 0 &&
               stayed->selection.caret == nenenib::core::Offset{2},
           "the line unit PgDn of a one-line text stays put as before");
}

void verify_vim_viewport_request()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport request sample parses");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView shown{buffer, selection, VimViewport{LineNumber{6}, 10, 0}};
    const VimEditorView hidden{buffer, selection, std::nullopt};
    verify_vim_viewport_reading_keys(shown, hidden);
    verify_vim_viewport_blind_keys(shown, hidden);
    verify_vim_wrapped_viewport_request();
    verify_vim_row_unit_pages();
    verify_indexed_virtual_columns();
    verify_indexed_columns_across_chunks();
}

} // namespace

// scope 専用の契約は verify_vim_scope_contracts の表が回す（Issue #97）。ここは scope を持たない
// Vim の共通部分だけを見る。
void verify_vim_engine()
{
    verify_vim_word_motions();
    verify_vim_forward_word_failures();
    verify_vim_caret_rules();
    verify_vim_redo_caret();
    verify_vim_step_edges();
    verify_vim_viewport_half_edges();
    verify_vim_viewport_page_edges();
    verify_vim_viewport_mode_edges();
    verify_vim_viewport_state_lifetime();
    verify_vim_viewport_request();
    verify_vim_visual_scroll_recovers_viewport();
    verify_vim_follow_at_document_end();
    verify_vim_insert_page_move_breaks_undo();
    verify_vim_visual_step_edges();
    verify_vim_key_notation();
    verify_vim_mode_labels();
    verify_vim_caret_shapes();
    verify_vim_visual_selection_and_clipboard();
    verify_vim_mode_entry();
    verify_vim_caret_placement();
    verify_vim_undo_boundaries();
    verify_vim_insert_undo_unit();
    verify_vim_change_undo_unit();
    verify_vim_insert_motion_breaks_the_unit();
    verify_vim_put_line_endings();
    verify_vim_crlf();
    verify_vim_other_keys();
    verify_vim_normal_stops_at_failure();
    verify_vim_tab_steps();
    verify_vim_fixtures();
}
} // namespace nenenib::tests
