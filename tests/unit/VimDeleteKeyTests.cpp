// Vim モードの Delete キー `<Del>` の controller の道（#1325）。engine の規則は fixture の
// `del-key-` が Vim 9.1 と照らし、ここは窓が送る意図（VimKeyPress・EditCommand）の行き先を守る。
#include "CaretMotion.hpp"
#include "CommandEdit.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "EditCommand.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "MoveCaret.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "TestSupport.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace application = nenenib::application;
namespace core = nenenib::core;

void press_delete(application::EditorController &controller)
{
    static_cast<void>(
        controller.apply(application::VimKeyPress{core::VimKey{core::VimSpecialKey::delete_key}}));
}

void edit_line(application::EditorController &controller, core::CommandEdit edit)
{
    static_cast<void>(controller.apply(application::EditCommand{edit}));
}

[[nodiscard]] std::string line_text(application::EditorController &controller)
{
    const auto view = controller.frame().command_line;
    return view.has_value() ? view.value().text : std::string("<closed>");
}

// NORMAL の `<Del>` は `x` と同じ。回数の途中の `<Del>` は最後の桁を消す（`12<Del>x` は 1 文字）。
void verify_normal_delete_key()
{
    Editing editing;
    open_vim_document(editing, "abcdefghijklmnop");
    auto &controller = editing.controller();
    press_delete(controller);
    expect(whole_vim_body(controller) == "bcdefghijklmnop" &&
               controller.vim_state().mode == core::VimMode::normal,
           "<Del> in NORMAL removes the character under the caret like x");
    vim_replay(controller, "12");
    press_delete(controller);
    vim_replay(controller, "x");
    expect(whole_vim_body(controller) == "cdefghijklmnop",
           "<Del> while typing a count drops its last digit (12<Del>x removes one character)");
}

// INSERT の `<Del>` は前方の Vim の 1 文字（結合文字ごと）を消し、行末では次の行とつなぐ。
void verify_insert_delete_key()
{
    Editing editing;
    open_vim_document(editing, "a\xCC\x81"
                               "bc\ndef");
    auto &controller = editing.controller();
    vim_replay(controller, "i");
    press_delete(controller);
    expect(whole_vim_body(controller) == "bc\ndef" &&
               controller.vim_state().mode == core::VimMode::insert,
           "<Del> in INSERT removes the next character with its combining mark");
    vim_replay(controller, "<End>");
    press_delete(controller);
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller) == "bcdef",
           "<Del> at the end of a line in INSERT joins the next line without a space");
}

[[nodiscard]] std::string raw_body(const application::EditorController &controller)
{
    return controller.text().text_range(core::Offset{0},
                                        core::Offset{controller.text().size_bytes()});
}

// CRLF の文書の行末の `A<Del>` は `\r\n` を丸ごと消し、`u` は改行の形ごと戻す（レビュー L3）。
// oracle は LF の本文しか測れないので、ここで生のバイト列を見る。
void verify_insert_delete_key_crlf()
{
    Editing editing;
    open_vim_document(editing, "ab\r\ncd");
    auto &controller = editing.controller();
    vim_replay(controller, "A");
    press_delete(controller);
    vim_replay(controller, "<Esc>");
    expect(raw_body(controller) == "abcd",
           "<Del> at the end of a CRLF line in INSERT removes the whole CRLF");
    vim_replay(controller, "u");
    expect(raw_body(controller) == "ab\r\ncd", "u restores the CRLF the <Del> removed");
}

// 通常モードの打鍵と Delete の undo の単位は、INSERT の `<Del>` を畳む枝（#1325 の absorbed）で
// 変わらない。打った文字は 1 単位、Delete は 1 回ずつ別の単位（ADR 0009 の決定 3）。
void verify_plain_delete_undo_units()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, application::VisibleLines{10});
    applied(controller, application::InsertText{"abc"});
    applied(controller, application::MoveCaret{core::CaretMotion::line_start,
                                               core::SelectionAnchoring::collapse});
    applied(controller, application::InsertText{"x"});
    applied(controller, application::DeleteText{core::DeleteDirection::forward});
    expect(applied(controller, application::HistoryAction{core::HistoryDirection::undo}) == "xabc",
           "a plain Delete right after typed text is its own undo unit");
    expect(applied(controller, application::HistoryAction{core::HistoryDirection::undo}) == "abc",
           "and the typed text before it stays a unit of its own");
    applied(controller, application::MoveCaret{core::CaretMotion::line_start,
                                               core::SelectionAnchoring::collapse});
    applied(controller, application::DeleteText{core::DeleteDirection::forward});
    applied(controller, application::DeleteText{core::DeleteDirection::forward});
    expect(applied(controller, application::HistoryAction{core::HistoryDirection::undo}) == "bc",
           "two plain Deletes in a row stay two undo units");
}

// `:` の行の Delete は Vim の c_<Del>。キャレットの下を消し、行末では手前を消し、空の行では閉じる。
void verify_command_line_delete_key()
{
    Editing editing;
    open_vim_document(editing, "abc");
    auto &controller = editing.controller();
    vim_replay(controller, ":set");
    edit_line(controller, core::CommandEdit::erase);
    expect(line_text(controller) == "se",
           "Delete at the end of the : line removes the character before");
    edit_line(controller, core::CommandEdit::left);
    edit_line(controller, core::CommandEdit::erase);
    expect(line_text(controller) == "s",
           "Delete inside the : line removes the character under the caret");
    vim_replay(controller, "<Del>");
    expect(line_text(controller).empty(),
           "the <Del> key from a replay edits the : line the same way");
    edit_line(controller, core::CommandEdit::erase);
    expect(!controller.command_line_active() && whole_vim_body(controller) == "abc",
           "Delete on an empty : line closes it like Backspace");
}
} // namespace

void verify_vim_delete_key_scope()
{
    verify_normal_delete_key();
    verify_insert_delete_key();
    verify_insert_delete_key_crlf();
    verify_plain_delete_undo_units();
    verify_command_line_delete_key();
}
} // namespace nenenib::tests
