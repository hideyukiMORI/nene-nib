// scope `--vim-text-objects` の単体テスト（ADR 0042 決定 2）。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineEnding.hpp"
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
#include "VimMoveTo.hpp"
#include "VimNoEffect.hpp"
#include "VimOperator.hpp"
#include "VimPendingOperator.hpp"
#include "VimPrefix.hpp"
#include "VimRegisterKind.hpp"
#include "VimRemoveRange.hpp"
#include "VimRepeatFailure.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimTextObject.hpp"
#include "VimTextObjectRange.hpp"
#include "VimTextObjectRequest.hpp"
#include "VimTextObjectScope.hpp"
#include "VimTextObjectSpan.hpp"
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
#include <utility>
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
using nenenib::core::LineEnding;
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
using nenenib::core::VimTextObject;
using nenenib::core::VimTextObjectRequest;
using nenenib::core::VimTextObjectScope;
using nenenib::core::VimTextObjectSpan;
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
    const VimEditorView view{buffer, collapsed_at(Offset{6}), VimViewport{LineNumber{1}, 8, 0}};
    VimState initial = empty_vim_state();
    initial.unnamed_register = nenenib::core::VimRegisterSnapshot::from(
        nenenib::core::VimRegister{"saved", VimRegisterKind::characters});
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
    const VimEditorView view{buffer, selected, VimViewport{LineNumber{1}, 8, 0}};
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
    expect(controller.vim_state().unnamed_register.value().text == "  body one\n  body two\n" &&
               controller.vim_state().unnamed_register.value().kind == VimRegisterKind::characters,
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
    expect(other.vim_state().unnamed_register.value().text == "alpha ",
           "a second iw grows the selection by one more chunk");
    vim_replay(other, "2lvi(y");
    expect(other.vim_state().unnamed_register.value().text == "p",
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

// 本文の最後の空行の `iw`（#1507）。CRLF の文書でも前の行の末尾の語と改行（`\r\n` ごと）を取り、
// レジスタの本文は LF の文書と同じ `b\n`（fixture は LF だけなので、CRLF の形はここで見る）。
void verify_vim_word_object_last_empty_line_crlf()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string{"c a.b\r\n"}});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "GciwX<Esc>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "c a.X",
           "ciw on the last empty line of a CRLF text takes the word before it and the CRLF");
    expect(controller.vim_state().unnamed_register.value().text == "b\n",
           "the register holds the word and an LF as for an LF text");
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
    expect(controller.vim_state().unnamed_register.value().text == "alpha beta gamma",
           "a count that runs out leaves the selection it managed to build (measured)");
    vim_replay(controller, "<Esc>02lvl9iwy");
    expect(controller.vim_state().unnamed_register.value().text == "pha beta gamma",
           "a forward selection keeps its anchor when the count runs out");
    vim_replay(controller, "<Esc>04lvhh9iwy");
    expect(controller.vim_state().unnamed_register.value().text == "alpha",
           "a backward selection keeps its anchor and the caret stops at the start of the text");
    vim_replay(controller, "<Esc>09lvhh9iwy");
    expect(controller.vim_state().unnamed_register.value().text == "alpha beta",
           "the backward walk crosses as many units as it can before it gives up");
    vim_replay(controller, "<Esc>02lV9iwy");
    expect(controller.vim_state().unnamed_register.value().kind == VimRegisterKind::lines,
           "a cancelled object in linewise VISUAL stays linewise");
    vim_replay(controller, "<Esc>0x");
    vim_replay(controller, "d9iw");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "a cancelled object moves the caret but is not a change for .");
    Editing blocks;
    open_vim_document(blocks, "if {a; b;} else");
    EditorController &other = blocks.controller();
    vim_replay(other, "5lvl9i{y");
    expect(other.vim_state().unnamed_register.value().text == "; ",
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
    expect(controller.vim_state().unnamed_register.value().text == "\"b",
           "a backward selection on the first quote of the line cancels (measured on Vim 9.1)");
    vim_replay(controller, "<Esc>02lvhi\"y");
    expect(controller.vim_state().unnamed_register.value().text == " \"",
           "a backward selection with no quote before the caret cancels");
    vim_replay(controller, "<Esc>012lvli\"y");
    expect(controller.vim_state().unnamed_register.value().text == "\" ",
           "a forward selection with no pair after the caret cancels");
    Editing odd;
    open_vim_document(odd, "it's a 'quoted' word");
    EditorController &other = odd.controller();
    vim_replay(other, "16lvhi'y");
    expect(other.vim_state().unnamed_register.value().text == " w",
           "a backward selection past the last unpaired quote cancels");
    Editing across;
    open_vim_document(across, "x \"aa\" y\nz \"bb\" w");
    EditorController &two_lines = across.controller();
    vim_replay(two_lines, "j7lvki\"y");
    expect(two_lines.vim_state().unnamed_register.value().text == "y\nz \"bb\" w",
           "a selection whose ends are on two lines cancels (quotes are read within one line)");
}

// 段落と文の VISUAL（Issue #729）。段落は行単位 VISUAL へ移り、選択を広げる形と文の広げ方は
// モードを変えない。本文の端で広げられない段落はビープが後続の鍵を捨てて 1 回の `:normal!` に
// 乗らないので（Issue #87）、固定 Vim を区切って測った値をここで固定する。
void verify_vim_text_object_paragraphs_and_sentences()
{
    Editing editing;
    open_vim_document(editing, "ab\ncd\n\nef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "lvip");
    expect(controller.vim_state().mode == VimMode::visual_line,
           "vip turns VISUAL linewise (measured against Vim 9.1)");
    vim_replay(controller, "<Esc>gglvj");
    vim_replay(controller, "ip");
    vim_replay(controller, "y");
    expect(controller.vim_state().unnamed_register.value().text == "b\ncd\n\n" &&
               controller.vim_state().unnamed_register.value().kind == VimRegisterKind::characters,
           "a multi-line characterwise selection grows by paragraphs and stays characterwise");
    Editing edge;
    open_vim_document(edge, "ab\ncd");
    EditorController &bottom = edge.controller();
    vim_replay(bottom, "lvjip");
    expect(bottom.vim_state().mode == VimMode::visual,
           "a paragraph that cannot grow past the end keeps VISUAL as it was");
    vim_replay(bottom, "y");
    expect(bottom.vim_state().unnamed_register.value().text == "b\nc",
           "the caret still moves to the start of the last line when the paragraph cannot grow");
    Editing sentences;
    open_vim_document(sentences, "One. Two. Three.");
    EditorController &other = sentences.controller();
    vim_replay(other, "4lVlis");
    expect(other.vim_state().mode == VimMode::visual_line,
           "is grows a linewise selection without leaving linewise VISUAL");
    vim_replay(other, "<Esc>0vis");
    expect(other.vim_state().mode == VimMode::visual, "vis selects the sentence characterwise");
}

// 文の走査は止まらない並びを本文の塊の走査で飛ばす（Issue #751）。塊の境が候補（`.`）の手前・上・
// 直後に来る本文でも、1 つの塊の本文と同じ答え（＝1 文字ずつ歩いた答え）になることを固定する。
// fixture の本文は 1 つの塊なので、塊をまたぐ分岐はここでだけ通る。
[[nodiscard]] std::optional<OffsetRange> sentence_range(const TextBuffer &text, std::size_t caret,
                                                        VimTextObjectScope scope, std::size_t count)
{
    const auto outcome = nenenib::core::vim_text_object_range(
        text, collapsed_at(Offset{caret}),
        VimTextObjectRequest{scope, VimTextObject::sentence, VimMode::normal}, count);
    return std::holds_alternative<VimTextObjectSpan>(outcome)
               ? std::optional<OffsetRange>{std::get<VimTextObjectSpan>(outcome).range.range}
               : std::nullopt;
}

[[nodiscard]] bool is_range(const std::optional<OffsetRange> &range, std::size_t begin,
                            std::size_t end)
{
    return range.has_value() && range.value().begin.value == begin &&
           range.value().end.value == end;
}

// 同じ文字を書き直して、[begin, end) の両端を塊の境にする。
[[nodiscard]] TextBuffer split_at(const TextBuffer &text, std::size_t begin, std::size_t end)
{
    return text.replaced(Offset{begin}, Offset{end}, text.text_range(Offset{begin}, Offset{end}));
}

// 全キャレット・is / as・回数 1〜3 で、塊を割った本文の答えが 1 つの塊の本文と同じか。
[[nodiscard]] bool sentences_match(const TextBuffer &whole, const TextBuffer &split)
{
    bool same = true;
    for (std::size_t trial = 0; trial < whole.size_bytes() * 6; ++trial)
    {
        const std::size_t caret = trial / 6;
        const VimTextObjectScope scope =
            trial % 2 == 0 ? VimTextObjectScope::inner : VimTextObjectScope::around;
        const std::size_t count = (trial % 6) / 2 + 1;
        same = same && sentence_range(whole, caret, scope, count) ==
                           sentence_range(split, caret, scope, count);
    }
    return same;
}

void verify_vim_sentence_across_pieces()
{
    // `.` は 7 と 19。前向きは 2 つめ、後ろ向きは 1 つめの `.` の周りの塊の境を越える。
    const auto whole = TextBuffer::from_utf8("One two. Three four.  Five six.").value();
    const TextBuffer before = split_at(split_at(whole, 4, 7), 16, 19);
    const TextBuffer on = split_at(split_at(whole, 7, 8), 19, 20);
    const TextBuffer after = split_at(split_at(whole, 8, 12), 20, 22);
    expect(before.piece_count() > 1 && on.piece_count() > 1 && after.piece_count() > 1,
           "rewriting the same characters splits the text into pieces");
    expect(is_range(sentence_range(before, 10, VimTextObjectScope::inner, 1), 9, 20) &&
               is_range(sentence_range(before, 10, VimTextObjectScope::around, 1), 9, 22) &&
               sentences_match(whole, before),
           "is and as find a candidate that starts a piece (Issue #751)");
    expect(is_range(sentence_range(on, 10, VimTextObjectScope::inner, 1), 9, 20) &&
               is_range(sentence_range(on, 10, VimTextObjectScope::around, 1), 9, 22) &&
               sentences_match(whole, on),
           "is and as find a candidate that is a piece of its own (Issue #751)");
    expect(is_range(sentence_range(after, 10, VimTextObjectScope::inner, 1), 9, 20) &&
               is_range(sentence_range(after, 10, VimTextObjectScope::around, 1), 9, 22) &&
               sentences_match(whole, after),
           "is and as find a candidate that ends a piece (Issue #751)");
}

// 前の行が文の終わりで行頭が `.` の行の `is` `as`（#1508）。CRLF の本文でも LF の fixture と同じく
// 次の行の文（`x1 foo.` と後ろの空白）を取る。塊を割った本文も同じ答え。
void verify_vim_sentence_line_head_crlf()
{
    const auto whole = TextBuffer::from_utf8("ab cd.\r\n.\r\nx1 foo. end").value();
    const TextBuffer split = split_at(whole, 8, 9);
    expect(whole.line_ending() == LineEnding::crlf, "the text is CRLF");
    expect(is_range(sentence_range(whole, 8, VimTextObjectScope::around, 1), 11, 19) &&
               is_range(sentence_range(whole, 8, VimTextObjectScope::inner, 1), 11, 18) &&
               sentences_match(whole, split),
           "is and as on a line that starts with a period after a finished sentence take the "
           "next sentence in a CRLF text (Issue #1508)");
}

void verify_vim_sentence_before_crlf()
{
    // CRLF の行の内容の終わりは `\r` の手前なので、`.` の次は行末として読まれ、`as` は LF の本文と
    // 同じく後ろの空白を持たない（範囲は `\r` の手前まで）。
    const auto whole = TextBuffer::from_utf8("One two.\r\nThree four.\r\n").value();
    const TextBuffer split = split_at(whole, 6, 8);
    expect(whole.line_ending() == LineEnding::crlf, "the text is CRLF");
    expect(is_range(sentence_range(split, 3, VimTextObjectScope::around, 1), 0, 8) &&
               is_range(sentence_range(split, 10, VimTextObjectScope::around, 1), 10, 21) &&
               sentences_match(whole, split),
           "as on a sentence that ends before a CRLF stops before the CR as LF text does (Issue "
           "#751)");
}

// `[` `]` の後ろの 1 鍵。族は未実装なので何も動かさずに失敗し、次の鍵を `{` `}` `(` `)` の移動や
// もう 1 つの `[` として流さない（Issue #1334・窓では失敗の後ろの鍵も走るので fixture
// では見えない）。
[[nodiscard]] bool bracket_refused(const VimState &state, const VimEditorView &view,
                                   char32_t prefix, char32_t key)
{
    const auto started = vim_step(state, view, VimKey{VimCharacter{prefix}});
    const auto refused = vim_step(started.next, view, VimKey{VimCharacter{key}});
    return waits_for_prefix(started.next, nenenib::core::VimPrefix::bracket) &&
           std::holds_alternative<nenenib::core::VimNoEffect>(started.effect) &&
           refused.failure == nenenib::core::VimRepeatFailure::refused &&
           std::holds_alternative<nenenib::core::VimNoEffect>(refused.effect) &&
           !refused.next.input_wait.has_value() && !refused.next.pending.has_value() &&
           !refused.next.count.has_value() && refused.next.mode == state.mode;
}

void verify_vim_bracket_prefix_refuses()
{
    const auto buffer = TextBuffer::from_utf8("a1\n\nb1 (c1). d1 {e1}\n\nf1").value();
    const VimEditorView view{buffer, collapsed_at(Offset{7}), VimViewport{LineNumber{1}, 8, 0}};
    constexpr std::array<std::pair<char32_t, char32_t>, 6> pairs{
        {{U'[', U'{'}, {U']', U'}'}, {U'[', U'('}, {U']', U')'}, {U'[', U'['}, {U']', U']'}}};
    const VimState normal = empty_vim_state();
    const auto counted = vim_step(normal, view, VimKey{VimCharacter{U'3'}}).next;
    const auto pending = vim_step(normal, view, VimKey{VimCharacter{U'd'}}).next;
    VimState visual = normal;
    visual.mode = VimMode::visual;
    for (const auto &[prefix, key] : pairs)
    {
        expect(bracket_refused(normal, view, prefix, key) &&
                   bracket_refused(counted, view, prefix, key),
               "[ and ] take the next key and fail without running it as its own command "
               "(Issue #1334)");
        expect(bracket_refused(pending, view, prefix, key),
               "an operator before [ or ] is cancelled and the text stays (Issue #1334)");
        expect(bracket_refused(visual, view, prefix, key),
               "[ and ] in VISUAL fail and keep the selection mode (Issue #1334)");
    }
}

// `[` `]` の後ろの Esc は失敗にせず打ち消すだけ（Vim は鳴らさず、`[<Esc>x` `d[<Esc>x` の x が
// 効く・`z<Esc>` と同じ・Issue #1344）。回数と保留中のオペレータも消える。
[[nodiscard]] bool bracket_escaped(const VimState &state, const VimEditorView &view,
                                   char32_t prefix)
{
    const auto started = vim_step(state, view, VimKey{VimCharacter{prefix}});
    const auto escaped = vim_step(started.next, view, VimKey{VimSpecialKey::escape});
    return waits_for_prefix(started.next, nenenib::core::VimPrefix::bracket) &&
           !escaped.failure.has_value() &&
           std::holds_alternative<nenenib::core::VimNoEffect>(escaped.effect) &&
           !escaped.next.input_wait.has_value() && !escaped.next.pending.has_value() &&
           !escaped.next.count.has_value() && escaped.next.mode == state.mode;
}

void verify_vim_bracket_escape_cancels()
{
    const auto buffer = TextBuffer::from_utf8("abc").value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 8, 0}};
    const VimState normal = empty_vim_state();
    const auto counted = vim_step(normal, view, VimKey{VimCharacter{U'3'}}).next;
    const auto pending = vim_step(normal, view, VimKey{VimCharacter{U'd'}}).next;
    for (const char32_t prefix : std::u32string_view{U"[]"})
    {
        expect(bracket_escaped(normal, view, prefix) && bracket_escaped(counted, view, prefix),
               "Esc after [ or ] cancels without failing so the next key runs (Issue #1344)");
        expect(bracket_escaped(pending, view, prefix),
               "Esc after d[ or d] cancels the operator without failing (Issue #1344)");
    }
}

// 本文の端を越える巨大な回数の `)` `(` `}` `{` は、端に着いた後の回数を空回りせずに失敗する
// （Vim の findsent の EQUAL_POS の枝と findpar・Issue #1334・止まらなければ CTest の TIMEOUT）。
void verify_vim_huge_count_motions_stop()
{
    std::string body;
    for (std::size_t paragraph = 0; paragraph < 64; ++paragraph)
    {
        body += "One two. Three four! Five?\nSix seven.\n\n";
    }
    body += "Eight. Nine.";
    const auto buffer = TextBuffer::from_utf8(body).value();
    const Offset end{buffer.size_bytes() - 1U};
    constexpr std::array<std::pair<char32_t, bool>, 4> motions{
        {{U')', true}, {U'(', false}, {U'}', true}, {U'{', false}}};
    for (const auto &[key, forward] : motions)
    {
        const Offset start = forward ? Offset{0} : end;
        const VimEditorView view{buffer, collapsed_at(start), VimViewport{LineNumber{1}, 8, 0}};
        VimState state = empty_vim_state();
        for (const char32_t digit : std::u32string_view{U"999999999"})
        {
            state = vim_step(state, view, VimKey{VimCharacter{digit}}).next;
        }
        const auto moved = vim_step(state, view, VimKey{VimCharacter{key}});
        const auto *const landed = std::get_if<nenenib::core::VimMoveTo>(&moved.effect);
        expect(moved.failure.has_value() && (landed == nullptr || landed->caret == start) &&
                   !moved.next.count.has_value(),
               "a count of 999999999 past the end of the text fails without walking the rest "
               "(Issue #1334)");
    }
}

// 巨大な回数の `is` `as` は、本文の端に着いた後の周を 2 周の繰り返しとして縮め、同じ偶奇の
// 小さな回数と同じ結果になる（Vim の findsent_forward は回数ぶん回る・Issue #1334・小さな回数の
// 側は fixture `count-repeat-1334-` が Vim と照らす）。
void verify_vim_huge_count_sentence_objects()
{
    constexpr std::string_view body =
        "One two. Three four! Five?\nSix seven (a [b] {c}) e.\n\nEight. Nine.\nlast line here";
    constexpr std::array<std::pair<std::string_view, std::string_view>, 6> pairs{
        {{"d999999999is", "d41is"},
         {"d999999999as", "d41as"},
         {"v999999999isd", "v41isd"},
         {"v999999999asd", "v41asd"},
         {"G$v999999999isd", "G$v41isd"},
         {"G$v999999999asd", "G$v41asd"}}};
    for (const auto &[huge, small] : pairs)
    {
        Editing hugely;
        open_vim_document(hugely, std::string{body});
        vim_replay(hugely.controller(), huge);
        Editing smally;
        open_vim_document(smally, std::string{body});
        vim_replay(smally.controller(), small);
        expect(whole_vim_body(hugely.controller()) == whole_vim_body(smally.controller()) &&
                   hugely.controller().vim_state().mode == smally.controller().vim_state().mode,
               "a count of 999999999 on is and as ends as a small count of the same parity "
               "(Issue #1334)");
    }
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
    expect(selected == 352,
           "the scope replays 346 text-object fixtures and 6 shared next-key boundaries");
}
} // namespace

void verify_vim_text_object_contracts()
{
    verify_vim_text_object_waiting();
    verify_vim_text_object_cancellation();
    verify_vim_text_object_visual();
    verify_vim_text_object_history();
    verify_vim_word_object_last_empty_line_crlf();
    verify_vim_text_object_dot();
    verify_vim_text_object_residuals();
    verify_vim_text_object_quote_pairs();
    verify_vim_text_object_paragraphs_and_sentences();
    verify_vim_sentence_across_pieces();
    verify_vim_sentence_before_crlf();
    verify_vim_sentence_line_head_crlf();
    verify_vim_bracket_prefix_refuses();
    verify_vim_bracket_escape_cancels();
    verify_vim_huge_count_motions_stop();
    verify_vim_huge_count_sentence_objects();
}

void verify_vim_text_object_scope()
{
    verify_vim_text_object_fixtures();
    verify_vim_text_object_contracts();
}
} // namespace nenenib::tests
