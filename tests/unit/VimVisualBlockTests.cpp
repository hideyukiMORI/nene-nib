// scope `--vim-visual-block` の単体テスト（ADR 0042 決定 2）。
#include "CaretShape.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "ModeLabel.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockExtent.hpp"
#include "VimBlockInsert.hpp"
#include "VimBlockLine.hpp"
#include "VimBlockRange.hpp"
#include "VimBlockWidth.hpp"
#include "VimColumnWish.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimReplay.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimVisualReselect.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnIndex.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
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
using nenenib::application::OpenDocument;
using nenenib::application::SaveDocument;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::CaretShape;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::mode_label;
using nenenib::core::Offset;
using nenenib::core::Selection;
using nenenib::core::SelectionPresence;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::vim_block_range;
using nenenib::core::vim_block_range_for;
using nenenib::core::vim_block_text;
using nenenib::core::vim_block_width;
using nenenib::core::vim_replayed_block_range;
using nenenib::core::vim_replayed_block_width;
using nenenib::core::vim_step;
using nenenib::core::vim_visual_reselect;
using nenenib::core::VimBlockEdit;
using nenenib::core::VimBlockExtent;
using nenenib::core::VimBlockInsert;
using nenenib::core::VimBlockLine;
using nenenib::core::VimBlockRange;
using nenenib::core::VimBlockWidth;
using nenenib::core::VimColumnWish;
using nenenib::core::VimEditorView;
using nenenib::core::VimEffect;
using nenenib::core::VimInsertBlock;
using nenenib::core::VimInsertString;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimMoveTo;
using nenenib::core::VimNavigate;
using nenenib::core::VimRemoveBlock;
using nenenib::core::VimReplaceBlock;
using nenenib::core::VimReplay;
using nenenib::core::VimSelect;
using nenenib::core::VimState;
using nenenib::core::VimStep;
using nenenib::core::VirtualColumn;

// ---------------------------------------------------------------- 矩形 VISUAL（ADR 0035）

[[nodiscard]] EditorController &vim_block_editor(Editing &editing, std::string_view text)
{
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(text)});
    static_cast<void>(controller.apply_frame(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    return controller;
}

// 矩形の選択は行ごとに描かれ、Ctrl+C / Ctrl+X も同じ行ごとの範囲を使う（決定 8）。fixture は
// 本文とレジスタしか見ないので、描く面と OS のクリップボードはここで測る。
void verify_vim_block_drawing()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "<C-v>jl");
    const auto frame = controller.frame();
    expect(frame.mode_label == "VISUAL BLOCK" && frame.caret.shape == CaretShape::block,
           "the block names itself on the status bar and keeps the block caret");
    expect(frame.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "the first row of the block is drawn");
    expect(frame.lines.at(1).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "so is the second row, at the same columns");
    expect(frame.lines.at(2).selection.presence == SelectionPresence::absent,
           "the row below the block is not drawn");
    static_cast<void>(applied(controller, ClipboardAction{ClipboardOperation::copy}));
    expect(editing.clipboard().read().value() == "ab\ngh",
           "Ctrl+C joins the rows with the document newline");
    static_cast<void>(applied(controller, ClipboardAction{ClipboardOperation::cut}));
    expect(vim_body(controller.frame()) == "cdef\nijkl\nmnopqr",
           "Ctrl+X takes the same rows out of the body");
}

// 行が矩形より手前で終わるとき。レジスタには幅ぶんの空白が入り本文は動かず、`r` も書かない
// （Issue #112 で実測）。この 3 つは 1 回の `:normal!` では再現しない（走査の失敗が残りの打鍵を
// 捨てる・Issue #87）ので fixture に採れず、ここで測る（out/issue112-oracle/add-fixtures.txt）。
void verify_vim_block_short_lines()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\ngh\nij");
    vim_replay(controller, "3l<C-v>jjly");
    expect(controller.vim_state().unnamed_register.value().text == "cd\n\n",
           "the rows that end before the block put nothing in the register");
    expect(vim_register_kind(controller.vim_state().unnamed_register.value()) == "\0262",
           "and the register is a block two columns wide");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{3}},
           "the yank leaves the caret at the top left corner");
    vim_replay(controller, "gg3l<C-v>jjld");
    expect(vim_body(controller.frame()) == "abef\ngh\nij",
           "and deleting the same block leaves those rows alone");
    vim_replay(controller, "ugg3l<C-v>jjlrZ");
    expect(vim_body(controller.frame()) == "abZZef\ngh\nij",
           "r writes nothing on the rows the block does not cover");
}

// 矩形の削除・置換・貼付は 1 つの undo 単位（決定 3）。oracle の `:normal!` は 1 回の実行を
// まるごと 1 単位にするので、単位の境はここで手で測る（ADR 0012 の決定 6）。
void verify_vim_block_undo_unit()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "x<C-v>jjlld");
    expect(vim_body(controller.frame()) == "ef\njkl\npqr", "x then a 3x3 block delete");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "bcdef\nghijkl\nmnopqr",
           "one undo takes back the whole block, all three rows at once");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "abcdef\nghijkl\nmnopqr",
           "the undo before it is the single x");
}

// CRLF の本文でも矩形は CR を本文に残さない（ADR 0012 の決定 9）。fixture は LF だけなので
// 保存したバイト列で測る。貼付が足す行も文書の改行で書く。
void verify_vim_block_line_endings()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abc\r\ndef\r\nghi");
    vim_replay(controller, "<C-v>jld");
    expect(controller.vim_state().unnamed_register.value().text == "ab\nde",
           "the block register holds LF only");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "c\r\nf\r\nghi",
           "the block delete kept the CRLF line endings");
    vim_replay(controller, "G$p");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "c\r\nf\r\nghiab\r\n   de",
           "and the line the block paste added came back as CRLF");
}

// 矩形では範囲外の鍵（決定 5）。どれも本文を変えず、矩形のまま残る。`~` `u` `U` は追補
// （#959）、`c` `I` `A` `C` `>` `<` は追補（#1042）で効くので block-case- と block-insert- の
// fixture が測る。
void verify_vim_block_unsupported_keys()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "yy<C-v>jl");
    constexpr std::array<std::string_view, 2> keys{"J", "p"};
    for (const std::string_view key : keys)
    {
        vim_replay(controller, key);
        expect(vim_body(controller.frame()) == "abcdef\nghijkl\nmnopqr",
               "a key that is out of scope for the block leaves the body alone");
        expect(controller.frame().mode_label == "VISUAL BLOCK", "and leaves the block selected");
    }
    vim_replay(controller, "iw");
    expect(controller.frame().mode_label == "VISUAL BLOCK",
           "a text object does not turn the block into a characterwise selection");
    expect(vim_body(controller.frame()) == "abcdef\nghijkl\nmnopqr", "and changes nothing");
}

// 矩形の INSERT（ADR 0035 の追補 #1042）。打つのは 1 行目だけで、Esc が CRLF の文書の残りの行へ
// 同じ桁で写し、1 行目の入力と合わせて 1 回の `u` で戻る。fixture は LF だけなのでここで測る。
void verify_vim_block_insert_document_newlines()
{
    Editing editing;
    const std::string original = "abc\r\ndef\r\nghi";
    EditorController &controller = vim_block_editor(editing, original);
    vim_replay(controller, "l<C-v>jjIXY");
    expect(vim_body(controller.frame()) == "aXYbc\ndef\nghi",
           "the block INSERT types on the first line only");
    vim_replay(controller, "<Esc>");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "aXYbc\r\ndXYef\r\ngXYhi",
           "Esc copies the typed text to every CRLF line at the same column");
    vim_replay(controller, "u");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == original, "one undo takes back the typing and the copies");
    vim_replay(controller, "0l<C-v>jlcZ<Esc>");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "aZ\r\ndZ\r\nghi", "blockwise c keeps CRLF");
    vim_replay(controller, "u");
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == original, "one undo takes back the block c");
}

// 矩形の INSERT の覚えは窓の割り込み（input_discarded）で消え、Esc はもう写さない（ARC-004・
// insert_repeat と同じ）。
void verify_vim_block_insert_interrupted()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "l<C-v>jjlcab");
    expect(controller.vim_state().block_insert.has_value(), "the block c remembers the copy");
    static_cast<void>(controller.apply_frame(SelectAll{}));
    expect(!controller.vim_state().block_insert.has_value(),
           "an interruption drops what the block INSERT would copy");
    vim_replay(controller, "<Esc>");
    expect(vim_body(controller.frame()) == "aabdef\ngjkl\nmpqr",
           "Esc after the interruption copies nothing to the other lines");
}

// core の純関数を直に測る。行ごとの範囲・左の桁・幅が 1 本で決まること（決定 2）。
void verify_vim_block_range()
{
    const auto text = TextBuffer::from_utf8("ab\tcd\nefghijkl\nmn\topq");
    expect(text.has_value(), "the block sample parses");
    if (!text.has_value())
    {
        return;
    }
    // 2 桁目の Tab は 3..8 桁を占める。矩形の角がその上に載ると Tab まるごとが矩形に入る。
    const Selection over_tab{Offset{2}, Offset{17}};
    const auto block = vim_block_range(text.value(), over_tab, VimColumnWish::at_column);
    expect(block.left == VirtualColumn{3} && block.width == VimBlockWidth{6},
           "the block spans the whole Tab");
    expect(block.lines.size() == 3 && block.first == LineNumber{1},
           "and covers the three lines the corners sit on");
    expect(vim_block_text(text.value(), block) == "\t\nghijkl\n\t",
           "the register text keeps the Tab and takes six columns from the plain line");
    const auto whole = vim_block_range(text.value(), over_tab, VimColumnWish::at_line_end);
    expect(whole.width == VimBlockWidth{9},
           "with $ the width comes from the longest line the block covers");
    expect(vim_block_text(text.value(), whole) == "\tcd\nghijkl\n\topq",
           "and every row runs to its own end");
}

// `.` の大きさ（決定 7）。左上をキャレットにして同じ行数と桁数の矩形を選び直す。
void verify_vim_block_reselect()
{
    const auto text = TextBuffer::from_utf8("abcd\nef\nijkl\nmn");
    expect(text.has_value(), "the reselect sample parses");
    if (!text.has_value())
    {
        return;
    }
    const auto columns = vim_visual_reselect(
        text.value(), Offset{9}, VimBlockExtent{2, VimColumnWish::at_column, VimBlockWidth{2}});
    expect(columns == Selection{Offset{9}, Offset{15}},
           "two lines and two columns from the third line's second column");
    const auto ends = vim_visual_reselect(
        text.value(), Offset{8}, VimBlockExtent{2, VimColumnWish::at_line_end, VimBlockWidth{2}});
    expect(ends == Selection{Offset{8}, Offset{15}},
           "$ puts the far end at the last line's content end and ignores the width");
}

// ---------------------------------------------------------------- 索引の道の同値（#1198）

// 矩形が 1 行を覆う形の 6 欄と、左の桁・幅・向き。
[[nodiscard]] bool same_block_range(const VimBlockRange &left, const VimBlockRange &right)
{
    return left.first == right.first && left.left == right.left && left.width == right.width &&
           left.wish == right.wish &&
           std::ranges::equal(left.lines, right.lines,
                              [](const VimBlockLine &one, const VimBlockLine &other)
                              {
                                  return one.range == other.range && one.inside == other.inside &&
                                         one.lead == other.lead && one.tail == other.tail &&
                                         one.keep_lead == other.keep_lead &&
                                         one.keep_tail == other.keep_tail;
                              });
}

[[nodiscard]] bool same_block_edits(const std::vector<VimBlockEdit> &left,
                                    const std::vector<VimBlockEdit> &right)
{
    return std::ranges::equal(left, right, [](const VimBlockEdit &one, const VimBlockEdit &other)
                              { return one.range == other.range && one.utf8 == other.utf8; });
}

// 矩形でない効果のうち、キャレット・選択・打った本文・`.` の鍵の列と選び直す大きさ（#1237）。
// ほかの効果は種類だけ。種類は呼ぶ側で比べてある。
[[nodiscard]] bool same_other_effect(const VimEffect &left, const VimEffect &right)
{
    if (const auto *moved = std::get_if<VimMoveTo>(&left))
    {
        return moved->caret == std::get<VimMoveTo>(right).caret;
    }
    if (const auto *selected = std::get_if<VimSelect>(&left))
    {
        return selected->selection == std::get<VimSelect>(right).selection;
    }
    if (const auto *typed = std::get_if<VimInsertString>(&left))
    {
        return typed->utf8 == std::get<VimInsertString>(right).utf8;
    }
    if (const auto *replay = std::get_if<VimReplay>(&left))
    {
        const auto &other = std::get<VimReplay>(right);
        return replay->keys == other.keys && replay->reselect == other.reselect;
    }
    return true;
}

// 矩形の鍵が起こす効果の中身（範囲と本文とキャレット）。ほかの効果は same_other_effect。
[[nodiscard]] bool same_block_effect(const VimEffect &left, const VimEffect &right)
{
    if (left.index() != right.index())
    {
        return false;
    }
    if (const auto *replaced = std::get_if<VimReplaceBlock>(&left))
    {
        const auto &other = std::get<VimReplaceBlock>(right);
        return replaced->caret == other.caret && same_block_edits(replaced->edits, other.edits);
    }
    if (const auto *removed = std::get_if<VimRemoveBlock>(&left))
    {
        const auto &other = std::get<VimRemoveBlock>(right);
        return removed->caret == other.caret && same_block_edits(removed->edits, other.edits);
    }
    if (const auto *inserted = std::get_if<VimInsertBlock>(&left))
    {
        const auto &other = std::get<VimInsertBlock>(right);
        return inserted->caret == other.caret && same_block_edits(inserted->edits, other.edits);
    }
    return same_other_effect(left, right);
}

[[nodiscard]] bool same_block_insert(const std::optional<VimBlockInsert> &left,
                                     const std::optional<VimBlockInsert> &right)
{
    if (!left.has_value() || !right.has_value())
    {
        return left.has_value() == right.has_value();
    }
    const VimBlockInsert &one = left.value();
    const VimBlockInsert &other = right.value();
    return one.first == other.first && one.lines == other.lines && one.side == other.side &&
           one.column == other.column && one.at_byte == other.at_byte &&
           one.line_bytes == other.line_bytes && one.caret == other.caret &&
           one.carried == other.carried && one.straddled == other.straddled;
}

// `.` の記録が覚えた矩形の大きさ（vim_block_width の道）。
[[nodiscard]] std::optional<VimBlockExtent> recorded_block(const VimState &state)
{
    if (!state.recording.has_value() || !state.recording.value().visual.has_value())
    {
        return std::nullopt;
    }
    const auto &extent = state.recording.value().visual.value();
    if (const auto *block = std::get_if<VimBlockExtent>(&extent))
    {
        return *block;
    }
    return std::nullopt;
}

[[nodiscard]] bool same_indexed_step(const VimStep &left, const VimStep &right)
{
    return same_block_effect(left.effect, right.effect) && left.next.mode == right.next.mode &&
           same_block_insert(left.next.block_insert, right.next.block_insert) &&
           left.restore == right.restore && left.failure.has_value() == right.failure.has_value() &&
           left.next.wanted_column == right.next.wanted_column &&
           left.next.replayed_block == right.next.replayed_block &&
           recorded_block(left.next) == recorded_block(right.next);
}

[[nodiscard]] TextBuffer with_block_edits(const TextBuffer &text, std::vector<VimBlockEdit> edits)
{
    std::ranges::sort(edits, [](const VimBlockEdit &one, const VimBlockEdit &other)
                      { return other.range.begin < one.range.begin; });
    TextBuffer next = text;
    for (const VimBlockEdit &edit : edits)
    {
        next = next.replaced(edit.range.begin, edit.range.end, edit.utf8);
    }
    return next;
}

// 試験の中で効果を本文と選択へ写す（controller の写しの小さな替え玉・矩形の鍵と INSERT
// の文字だけ）。
[[nodiscard]] std::pair<TextBuffer, Selection>
block_applied(const TextBuffer &text, const Selection &selection, const VimEffect &effect)
{
    if (const auto *moved = std::get_if<VimMoveTo>(&effect))
    {
        return {text, collapsed_at(moved->caret)};
    }
    if (const auto *selected = std::get_if<VimSelect>(&effect))
    {
        return {text, selected->selection};
    }
    if (const auto *navigated = std::get_if<VimNavigate>(&effect))
    {
        return {text, navigated->selection};
    }
    if (const auto *typed = std::get_if<VimInsertString>(&effect))
    {
        return {text.insert(selection.caret, typed->utf8),
                collapsed_at(Offset{selection.caret.value + typed->utf8.size()})};
    }
    if (const auto *replaced = std::get_if<VimReplaceBlock>(&effect))
    {
        return {with_block_edits(text, replaced->edits), collapsed_at(replaced->caret)};
    }
    if (const auto *removed = std::get_if<VimRemoveBlock>(&effect))
    {
        return {with_block_edits(text, removed->edits), collapsed_at(removed->caret)};
    }
    if (const auto *inserted = std::get_if<VimInsertBlock>(&effect))
    {
        return {with_block_edits(text, inserted->edits), collapsed_at(inserted->caret)};
    }
    return {text, selection};
}

// 矩形の VISUAL のあいだの照らし。frame と同じ vim_block_range_for の形と記録の vim_block_width、
// `.` の再生の中なら再生の矩形（vim_replayed_block_range）の幅と記録し直す幅
// （vim_replayed_block_width・#1237）。どちらの本文でも同じ答えか。状態（欲しい列と再生の大きさ）
// は 2 つの本文で同じことを same_indexed_step が鍵ごとに照らしている。
[[nodiscard]] bool same_drawn_block(const TextBuffer &plain, const TextBuffer &indexed,
                                    const Selection &selection, const VimState &state)
{
    if (state.mode != VimMode::visual_block)
    {
        return true;
    }
    const auto block = vim_block_range_for(plain, selection, state.wanted_column);
    const bool drawn =
        same_block_range(block, vim_block_range_for(indexed, selection, state.wanted_column)) &&
        vim_block_width(indexed, selection, state.wanted_column) == block.width &&
        vim_block_width(plain, selection, state.wanted_column) == block.width;
    if (!state.replayed_block.has_value())
    {
        return drawn;
    }
    const VimBlockExtent extent = state.replayed_block.value();
    const VimBlockWidth replayed = vim_replayed_block_range(plain, selection, extent).width;
    return drawn && vim_replayed_block_width(plain, selection, extent) == replayed &&
           vim_replayed_block_width(indexed, selection, extent) == replayed;
}

// `.` の再生（ADR 0033 の決定 4）。controller と同じく選び直してから、再生の鍵を列の頭へ
// 差し込む。ほかの効果では何もしない。
void queued_replay(const TextBuffer &text, const VimEffect &effect, Selection &selection,
                   std::deque<VimKey> &queue)
{
    const auto *replay = std::get_if<VimReplay>(&effect);
    if (replay == nullptr)
    {
        return;
    }
    if (replay->reselect.has_value())
    {
        selection = vim_visual_reselect(text, selection.caret, replay->reselect.value());
    }
    queue.insert(queue.begin(), replay->keys.begin(), replay->keys.end());
}

// 同じ本文の「索引を持たない本文」と「持つ本文」に同じ鍵を流し、鍵ごとの答えが同じか。矩形の
// VISUAL のあいだは frame と同じ vim_block_range_for と、記録の vim_block_width も照らす。`.` は
// controller と同じく選び直してから再生の鍵を列の頭へ差し込んで流す（再生の中の記録の幅も照らす・
// #1237）。比べた鍵の数を compared に足す。
[[nodiscard]] bool indexed_block_keys_agree(std::string_view body, Offset start,
                                            std::string_view keys, std::size_t &compared)
{
    const auto parsed = TextBuffer::from_utf8(body);
    if (!parsed.has_value())
    {
        return false;
    }
    TextBuffer plain = parsed.value();
    TextBuffer indexed = plain.with_virtual_columns();
    Selection selection = collapsed_at(start);
    VimState plain_state = empty_vim_state();
    VimState indexed_state = empty_vim_state();
    bool same = true;
    const std::vector<VimKey> typed = vim_keys_of(keys);
    std::deque<VimKey> queue(typed.begin(), typed.end());
    while (!queue.empty())
    {
        const VimKey key = queue.front();
        queue.pop_front();
        same = same && same_drawn_block(plain, indexed, selection, plain_state);
        const VimStep plain_step =
            vim_step(plain_state, VimEditorView{plain, selection, std::nullopt}, key);
        const VimStep indexed_step =
            vim_step(indexed_state, VimEditorView{indexed, selection, std::nullopt}, key);
        same = same && same_indexed_step(plain_step, indexed_step);
        auto [next_plain, next_selection] = block_applied(plain, selection, plain_step.effect);
        indexed = block_applied(indexed, selection, plain_step.effect).first;
        plain = std::move(next_plain);
        selection = next_selection;
        plain_state = plain_step.next;
        indexed_state = indexed_step.next;
        queued_replay(plain, plain_step.effect, selection, queue);
        ++compared;
    }
    return same && indexed.has_virtual_columns() && plain.text() == indexed.text();
}

// 矩形の入口の鍵（`I` `A` `c` `C` `d` `y` `r` `~` と Esc の写し）。
constexpr std::array<std::string_view, 12> indexed_block_keys{
    "<C-v>jIxy<Esc>",  "<C-v>jlAz<Esc>", "$<C-v>jAq<Esc>", "<C-v>3j2Ap<Esc>",
    "<C-v>jlcnn<Esc>", "<C-v>jCm<Esc>",  "<C-v>3jld",      "<C-v>3jly",
    "<C-v>2jlrZ",      "<C-v>jl~",       "$<C-v>3jI<Esc>", "<C-v>3j$Aw<Esc>"};

// 矩形の `r`（`$` の矩形）・矩形のレジスタの `p` `P`（回数・行末の後ろ）・`.` の再生（`I` `A` `c`
// `C` `r` `d` の後）・Esc の 1 文字戻りと `h`（#1237）。
constexpr std::array<std::string_view, 14> indexed_rest_keys{
    "$<C-v>jrZ",         "<C-v>3j$rQ",       "<C-v>jlyjp",       "<C-v>3jlyP",       "<C-v>jly$p",
    "<C-v>jly2p",        "<C-v>jIxy<Esc>j.", "<C-v>jlAz<Esc>l.", "$<C-v>jAq<Esc>j.", "<C-v>jlrZj.",
    "<C-v>jlcnn<Esc>j.", "<C-v>jCm<Esc>j.",  "ix<Esc>",          "a<Esc>3h"};

// 1 つの角から鍵の列を全部流す。
[[nodiscard]] bool indexed_keys_agree_at(std::string_view body, Offset at,
                                         std::span<const std::string_view> lists,
                                         std::size_t &compared)
{
    bool same = true;
    for (const std::string_view keys : lists)
    {
        same = indexed_block_keys_agree(body, at, keys, compared) && same;
    }
    return same;
}

// 1 行目と 2 行目の code point ごとに矩形の角を置く（改行と CRLF の `\r` は飛ばす）。
[[nodiscard]] bool indexed_body_agrees(std::string_view body,
                                       std::span<const std::string_view> lists,
                                       std::size_t &compared)
{
    const std::size_t second = body.find('\n') + 1;
    const std::size_t third = body.find('\n', second);
    bool same = true;
    for (std::size_t at = 0; at < third; ++at)
    {
        const bool corner =
            nenenib::core::is_boundary(body, Offset{at}) && body.at(at) != '\n' &&
            !(body.at(at) == '\r' && at + 1 < body.size() && body.at(at + 1) == '\n');
        same = (!corner || indexed_keys_agree_at(body, Offset{at}, lists, compared)) && same;
    }
    return same;
}

// 小さな本文: Tab の途中・全角の途中・結合文字・`^V` と `\r` の 2
// 桁の制御文字・短い行・空行・CRLF。
constexpr std::array<std::string_view, 4> indexed_bodies{
    "ab\tcd\xE6\x97\xA5"
    "ef\x16gh\n\t\xE5\x85\xA8gh\x16ij\re\xCC\x81z\nx\n\nab\rcde\xCC\x81"
    "fg\tk\n",
    "\xE6\x97\xA5\t\xE6\x97\xA5x\n\t\t\tend\ne\xCC\x81\xCC\x81\t\xE5\x85\xA8\nshort",
    "ab\tc\xE6\x97\xA5\r\n\t\xE5\x85\xA8"
    "d\r\nxy\r\n\r\nlonger line\r\n",
    // Tab と `e` の後に U+0301 が 40 個（80 バイト）。境を探す窓（64 バイト）を倍にする。
    "a\t"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "bc\ne"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "fg\t\nxyz"};

void verify_vim_block_indexed_small()
{
    std::size_t compared = 0;
    bool same = true;
    for (const std::string_view body : indexed_bodies)
    {
        same = indexed_body_agrees(body, indexed_block_keys, compared) && same;
    }
    expect(same, "an indexed text answers every blockwise key the same as a copied line");
    expect(compared == 8960, "the blockwise equivalence compares 8960 keys");
}

// 同じ小さな本文で、矩形の `r` `p` と `.` の再生と Esc の 1 文字戻り（#1237）。
void verify_vim_block_indexed_rest()
{
    std::size_t compared = 0;
    bool same = true;
    for (const std::string_view body : indexed_bodies)
    {
        same = indexed_body_agrees(body, indexed_rest_keys, compared) && same;
    }
    expect(same, "an indexed text answers r, p, . and Esc in a block the same as a copied line");
    expect(compared == 13824, "the rest of the blockwise equivalence compares 13824 keys");
}

// 塊の境 `boundary` の前後（-64・0・+63 の次の code point）に矩形の角を置く。
[[nodiscard]] bool indexed_boundary_agrees(std::string_view body, std::size_t boundary,
                                           std::span<const std::string_view> lists,
                                           std::size_t &compared)
{
    bool same = true;
    for (const std::size_t near : {boundary - 64, boundary, boundary + 63})
    {
        std::size_t at = near;
        while (!nenenib::core::is_boundary(body, Offset{at}))
        {
            ++at;
        }
        for (const std::string_view keys : lists)
        {
            same = indexed_block_keys_agree(body, Offset{at}, keys, compared) && same;
        }
    }
    return same;
}

// 塊（約 64 KiB）の境の前後に矩形の端を置く（200 KB の行 2 本）。1 行目の中の最初の境だけ。
void verify_vim_block_indexed_across_chunks()
{
    std::string line;
    while (line.size() < 200'000)
    {
        line += "ab\t\xE6\x97\xA5"
                "e\xCC\x81\x01z\t";
    }
    const std::string body = line + "\n" + line;
    const auto parsed = TextBuffer::from_utf8(body);
    expect(parsed.has_value(), "the chunked block sample parses");
    const TextBuffer indexed = parsed.value().with_virtual_columns();
    const auto chunks = indexed.virtual_columns().value().get().chunks();
    const auto inside =
        std::ranges::find_if(chunks, [&line](const auto &chunk)
                             { return chunk.start > 0 && chunk.start + 64 < line.size(); });
    expect(inside != chunks.end(), "a chunk boundary falls inside the first line");
    // 入口の 2 列（#1198）と、`r` `p` `.` と Esc の 4 列（#1237）。
    constexpr std::array<std::string_view, 6> lists{"<C-v>jIxy<Esc>",   "<C-v>jlAz<Esc>",
                                                    "<C-v>jlrZ",        "<C-v>jlyjp",
                                                    "<C-v>jIxy<Esc>j.", "ix<Esc>"};
    std::size_t compared = 0;
    bool same =
        inside == chunks.end() || indexed_boundary_agrees(body, inside->start, lists, compared);
    same = indexed_block_keys_agree(body, Offset{0}, "$<C-v>jAq<Esc>", compared) && same;
    expect(same, "the blockwise keys agree when the block edges sit near a chunk boundary");
}

void verify_vim_block_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("block-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 431, "the scope replays its 431 blockwise oracle fixtures");
}
} // namespace

void verify_vim_block_contracts()
{
    verify_vim_block_drawing();
    verify_vim_block_short_lines();
    verify_vim_block_undo_unit();
    verify_vim_block_line_endings();
    verify_vim_block_unsupported_keys();
    verify_vim_block_insert_document_newlines();
    verify_vim_block_insert_interrupted();
    verify_vim_block_range();
    verify_vim_block_reselect();
    verify_vim_block_indexed_small();
    verify_vim_block_indexed_rest();
    verify_vim_block_indexed_across_chunks();
}

void verify_vim_block_scope()
{
    verify_vim_block_fixtures();
    verify_vim_block_contracts();
}
} // namespace nenenib::tests
