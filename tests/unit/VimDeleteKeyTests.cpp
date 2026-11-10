// Vim モードの Delete キー `<Del>` の controller の道（#1325）。engine の規則は fixture の
// `del-key-` が Vim 9.1 と照らし、ここは窓が送る意図（VimKeyPress・EditCommand）の行き先を守る。
#include "CommandEdit.hpp"
#include "EditCommand.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"

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
    verify_command_line_delete_key();
}
} // namespace nenenib::tests
