// scope `--vim-eol` の単体テスト（ADR 0188）。改行で終わる本文を Vim のモードは最後の改行を
// 除いて見る（旗・入口・縁 3 つ・画面）。通常モードは最後の空の行に居られる（D82）。
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "VimRegisterKind.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::SaveDocument;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::Column;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::SelectionAnchoring;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::VimRegisterKind;

void open_in(Editing &editing, std::string_view text, EditMode mode)
{
    editing.files().hold(Bytes{std::string(text)});
    applied(editing.controller(), VisibleLines{vim_visible_lines});
    applied(editing.controller(), OpenDocument{sample_path()});
    applied(editing.controller(), SelectEditMode{mode});
}

[[nodiscard]] std::string saved(Editing &editing)
{
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    return editing.files().written();
}

[[nodiscard]] std::size_t caret_line(EditorController &controller)
{
    return controller.frame().caret.position.line.value;
}

// 入口（決定 2・4）。engine と画面は最後の空の行を数えず、`G` `:$` は最後の行、`/^$` は当たらない。
void verify_vim_eol_entry()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_in(editing, "a\nb\n", EditMode::vim);
    expect(controller.frame().total_lines == 2 && controller.frame().hidden_line_break,
           "Vim mode shows two lines of an eol body and keeps the final break hidden");
    vim_replay(controller, "G");
    expect(caret_line(controller) == 2, "G lands on the last line, not the empty line after it");
    vim_replay(controller, "gg:$<CR>");
    expect(caret_line(controller) == 2, ":$ is the last line");
    vim_replay(controller, "gg/^$<CR>");
    expect(caret_line(controller) == 1, "/^$ finds no empty line at the end");
    vim_replay(controller, ":g/^$/d<CR>");
    expect(saved(editing) == "a\nb\n", ":g/^$/d keeps the final line break");
}

// 通常モードは最後の空の行に居られ、Vim へ入るとキャレットは最後の行へ上がる（決定 4・D82）。
// 戻ると最後の空の行がまた見える。
void verify_vim_eol_switch()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_in(editing, "a\nb\n", EditMode::ordinary);
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{3}, Column{1}}, SelectionAnchoring::collapse});
    expect(controller.frame().total_lines == 3 && caret_line(controller) == 3 &&
               !controller.frame().hidden_line_break,
           "the ordinary mode keeps the empty line after the final break");
    applied(controller, SelectEditMode{EditMode::vim});
    expect(controller.frame().total_lines == 2 && caret_line(controller) == 2,
           "entering Vim mode lifts the caret to the last line");
    applied(controller, SelectEditMode{EditMode::ordinary});
    expect(controller.frame().total_lines == 3, "leaving Vim mode shows the empty line again");
    // 通常モードで改行を消してから入ると旗は立たない（入るときに決め直す・決定 1）。
    Editing plain;
    open_in(plain, "a\nb", EditMode::ordinary);
    applied(plain.controller(), SelectEditMode{EditMode::vim});
    expect(plain.controller().frame().total_lines == 2 &&
               !plain.controller().frame().hidden_line_break,
           "a body without the final break has no flag");
}

// 保存のバイト列は変わらない（決定 5）。編集は最後の改行の手前に入り、CRLF も同じ。
void verify_vim_eol_saved_bytes()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_in(editing, "a\nb\n", EditMode::vim);
    vim_replay(controller, "GAz<Esc>");
    expect(saved(editing) == "a\nbz\n", "A appends before the final break");
    vim_replay(controller, "Goy<Esc>");
    expect(saved(editing) == "a\nbz\ny\n", "o opens a line before the final break");
    Editing crlf;
    EditorController &crlf_controller = crlf.controller();
    open_in(crlf, "a\r\nb\r\n", EditMode::vim);
    expect(crlf_controller.frame().total_lines == 2, "a CRLF body hides its two-byte final break");
    vim_replay(crlf_controller, "Goz<Esc>");
    expect(saved(crlf) == "a\r\nb\r\nz\r\n", "the CRLF body keeps its final break");
    vim_replay(crlf_controller, "Gdd");
    expect(saved(crlf) == "a\r\nb\r\n" &&
               crlf_controller.vim_state().unnamed_register.value().text == "z\n",
           "dd on the last line of a CRLF body takes that line with its break");
}

// 全部の行を消すと空の本文で旗が下り（縁 a）、足した本文は改行で終わらない（縁 b）。undo で
// 消す前に戻ると旗も戻る。
void verify_vim_eol_emptied()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_in(editing, "a\nb\n", EditMode::vim);
    vim_replay(controller, "ggdG");
    expect(saved(editing).empty() &&
               controller.vim_state().unnamed_register.value().text == "a\nb\n" &&
               controller.vim_state().unnamed_register.value().kind == VimRegisterKind::lines,
           "ggdG leaves an empty body and yanks the two lines");
    vim_replay(controller, "oz<Esc>");
    expect(saved(editing) == "\nz" && controller.frame().total_lines == 2,
           "after the flag is lowered, lines added to the empty body do not end in a break");
    vim_replay(controller, "uu");
    expect(saved(editing) == "a\nb\n", "two undos bring the body back");
    vim_replay(controller, "G");
    expect(controller.frame().total_lines == 2 && caret_line(controller) == 2,
           "and the flag comes back with it");
    vim_replay(controller, "<C-r>");
    expect(saved(editing).empty(), "redo deletes every line again");
    vim_replay(controller, "ux");
    expect(saved(editing) == "\nb\n", "an edit after the undo drops the redo");
    vim_replay(controller, "G");
    expect(controller.frame().hidden_line_break && caret_line(controller) == 2,
           "the flag stays up once the lowering unit is gone");
    Editing single;
    open_in(single, "a\n", EditMode::vim);
    vim_replay(single.controller(), ":%d<CR>");
    expect(saved(single).empty() && single.controller().frame().total_lines == 1,
           ":%d empties a one-line body too");
}

// 空の 1 行（`\n`）は engine に「1 行ある」と添える（縁 c）。`:d` はレジスタに `\n` を書く。
void verify_vim_eol_one_empty_line()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_in(editing, "\n", EditMode::vim);
    expect(controller.frame().total_lines == 1, "the body \\n is one empty line");
    vim_replay(controller, ":d<CR>");
    expect(saved(editing).empty() && controller.vim_state().unnamed_register.value().text == "\n" &&
               controller.vim_state().unnamed_register.value().kind == VimRegisterKind::lines,
           ":d on the one empty line yanks it and leaves an empty body");
    vim_replay(controller, "u");
    expect(saved(editing) == "\n" && controller.frame().hidden_line_break,
           "undo restores the empty line and its flag");
    vim_replay(controller, "dd");
    expect(saved(editing).empty(), "dd deletes the one empty line as Vim does");
}

// 再生（`@a`）の中で全部の行を消したら、畳んだ単位の上でも旗は下りたまま（決定 3 (a)）。
void verify_vim_eol_replayed()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_in(editing, "ab\ncd\n", EditMode::vim);
    vim_replay(controller, "qaxggdGquu");
    expect(saved(editing) == "ab\ncd\n", "recording and two undos restore the body");
    vim_replay(controller, "@a");
    expect(saved(editing).empty(), "the macro deletes every line");
    vim_replay(controller, "oz<Esc>");
    expect(saved(editing) == "\nz", "the flag stays lowered after the folded unit");
    vim_replay(controller, "uu");
    expect(saved(editing) == "ab\ncd\n" && controller.frame().total_lines == 2,
           "undoing the folded unit brings the body and the flag back");
}
} // namespace

void verify_vim_eol_contracts()
{
    verify_vim_eol_entry();
    verify_vim_eol_switch();
    verify_vim_eol_saved_bytes();
    verify_vim_eol_emptied();
    verify_vim_eol_one_empty_line();
    verify_vim_eol_replayed();
}

void verify_vim_eol_scope()
{
    verify_vim_eol_contracts();
}
} // namespace nenenib::tests
