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
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VimBlockExtent.hpp"
#include "VimBlockRange.hpp"
#include "VimBlockWidth.hpp"
#include "VimColumnWish.hpp"
#include "VimTestSupport.hpp"
#include "VimVisualReselect.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::application::ClipboardAction;
using nenenib::application::ClipboardOperation;
using nenenib::application::OpenDocument;
using nenenib::application::SaveDocument;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::CaretShape;
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
using nenenib::core::vim_block_text;
using nenenib::core::vim_visual_reselect;
using nenenib::core::VimBlockExtent;
using nenenib::core::VimBlockWidth;
using nenenib::core::VimColumnWish;
using nenenib::core::VirtualColumn;

// ---------------------------------------------------------------- 矩形 VISUAL（ADR 0035）

[[nodiscard]] EditorController &vim_block_editor(Editing &editing, std::string_view text)
{
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(text)});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
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
    expect(controller.vim_state().unnamed_register.text == "cd\n\n",
           "the rows that end before the block put nothing in the register");
    expect(vim_register_kind(controller.vim_state().unnamed_register) == "\0262",
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
    expect(controller.vim_state().unnamed_register.text == "ab\nde",
           "the block register holds LF only");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "c\r\nf\r\nghi",
           "the block delete kept the CRLF line endings");
    vim_replay(controller, "G$p");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "c\r\nf\r\nghiab\r\n   de",
           "and the line the block paste added came back as CRLF");
}

// 矩形では範囲外の鍵（決定 5）。どれも本文を変えず、矩形のまま残る。
void verify_vim_block_unsupported_keys()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "yy<C-v>jl");
    constexpr std::array<std::string_view, 8> keys{"c", "I", "A", "C", ">", "J", "~", "p"};
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
    expect(selected == 144, "the scope replays its 144 blockwise oracle fixtures");
}
} // namespace

void verify_vim_block_contracts()
{
    verify_vim_block_drawing();
    verify_vim_block_short_lines();
    verify_vim_block_undo_unit();
    verify_vim_block_line_endings();
    verify_vim_block_unsupported_keys();
    verify_vim_block_range();
    verify_vim_block_reselect();
}

void verify_vim_block_scope()
{
    verify_vim_block_fixtures();
    verify_vim_block_contracts();
}
} // namespace nenenib::tests
