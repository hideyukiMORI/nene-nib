// Issue #1050 / ADR 0175。F5 の日時の挿入と現在時刻の port。時刻は替え玉 ScriptedClock が置いた
// 値だけを読み（実時刻を読まない）、controller が now() を呼ぶのは InsertDateTime のときだけ。
#include "AutosaveTick.hpp"
#include "CancelCommand.hpp"
#include "DateTimeText.hpp"
#include "Editing.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertDateTime.hpp"
#include "InsertText.hpp"
#include "LocalDateTime.hpp"
#include "Scopes.hpp"
#include "SelectAll.hpp"
#include "TestSupport.hpp"
#include "VimMode.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

constexpr std::string_view placed = "12:34 2026/10/29";
constexpr std::string_view insert_only = "INSERT モードで F5 を押してください";

[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

// 決定 2: 形は `HH:mm yyyy/MM/dd` で 0 埋め。秒は出さない。
void verify_text_form()
{
    expect(core::date_time_text(core::LocalDateTime{2026, 10, 29, 12, 34, 56}) == placed,
           "the date and time read HH:mm yyyy/MM/dd");
    expect(core::date_time_text(core::LocalDateTime{2026, 1, 2, 3, 4, 5}) == "03:04 2026/01/02",
           "hours, minutes, months and days are padded with zeros to two digits");
    expect(core::date_time_text(core::LocalDateTime{987, 12, 31, 0, 0, 59}) == "00:00 0987/12/31",
           "the year is padded to four digits and the seconds are not shown");
    expect(core::LocalDateTime{2026, 10, 29, 12, 34, 56} ==
               core::LocalDateTime{2026, 10, 29, 12, 34, 56},
           "two equal date and time values compare equal");
}

// 決定 3（通常モード）: キャレットに入れる。InsertText と同じ Edit 1 回で、undo 1 回で消える。
void verify_ordinary_insert()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    expect(applied(controller, app::InsertDateTime{}) == placed &&
               editing.wall_clock().calls() == 1,
           "F5 in the ordinary mode inserts the clock's value at the caret, reading it once");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}).empty(),
           "one undo removes the inserted date and time");
    editing.wall_clock().set(core::LocalDateTime{2027, 3, 4, 5, 6, 7});
    applied(controller, app::InsertText{"hello"});
    applied(controller, app::SelectAll{});
    expect(applied(controller, app::InsertDateTime{}) == "05:06 2027/03/04" &&
               editing.wall_clock().calls() == 2,
           "F5 replaces the selection with the date and time");
}

// 決定 3（Vim）: INSERT は打鍵として engine へ（undo は INSERT 1 回のまま）。NORMAL と VISUAL は
// 入れずに 1 行知らせ、鳴らさず、時刻を読まない。
void verify_vim()
{
    Editing editing;
    open_vim_document(editing, "ab\n");
    auto &controller = editing.controller();
    applied(controller, app::InsertDateTime{});
    expect(
        vim_body(controller.frame()) == "ab\n" && notice(controller.frame()) == insert_only &&
            editing.wall_clock().calls() == 0 && editing.bell().rings() == 0,
        "F5 in Vim NORMAL inserts nothing, says so in one line, never rings nor reads the clock");
    vim_replay(controller, "v");
    applied(controller, app::InsertDateTime{});
    expect(vim_body(controller.frame()) == "ab\n" && notice(controller.frame()) == insert_only &&
               controller.frame().vim_mode == core::VimMode::visual &&
               editing.wall_clock().calls() == 0,
           "F5 in Vim VISUAL inserts nothing and keeps the selection mode");
    vim_replay(controller, "<Esc>i");
    applied(controller, app::InsertDateTime{});
    vim_replay(controller, "x<Esc>");
    expect(vim_body(controller.frame()) == std::string(placed) + "xab\n" &&
               editing.wall_clock().calls() == 1,
           "F5 in Vim INSERT types the date and time as keys before the caret");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "ab\n",
           "one u removes the whole INSERT together with the date and time");
}

// #1120 の R3-1: オペレータの待ちの途中の F5 は、知らせと一緒に待ちを取り消す（Esc と同じ道）。
// 続く j は削除ではなく移動になる。VISUAL の F5 は選択を残す。
void verify_vim_waiting()
{
    Editing editing;
    open_vim_document(editing, "ab\ncd\n");
    auto &controller = editing.controller();
    vim_replay(controller, "d");
    applied(controller, app::InsertDateTime{});
    expect(!controller.vim_state().pending.has_value() &&
               notice(controller.frame()) == insert_only && editing.wall_clock().calls() == 0,
           "F5 after an operator cancels the operator and says so in one line");
    vim_replay(controller, "jx");
    expect(vim_body(controller.frame()) == "ab\nd\n",
           "the j after the cancelled operator moves instead of deleting lines");
    vim_replay(controller, "ggvl");
    applied(controller, app::InsertDateTime{});
    vim_replay(controller, "d");
    expect(vim_body(controller.frame()) == "\nd\n" && editing.wall_clock().calls() == 0,
           "F5 in VISUAL keeps the selection, so the next d deletes it");
}

// 決定 3（入力行）: `:` と検索の行が開いていれば、入力行に文字として入れる。本文は変えない。
void verify_input_line()
{
    Editing editing;
    open_vim_document(editing, "ab\n");
    auto &controller = editing.controller();
    vim_replay(controller, ":");
    applied(controller, app::InsertDateTime{});
    const auto frame = controller.frame();
    expect(frame.command_line.has_value() && frame.command_line.value().text == placed &&
               vim_body(frame) == "ab\n" && editing.wall_clock().calls() == 1,
           "F5 with the Ex line open types the date and time into the line");
    applied(controller, app::CancelCommand{});
    vim_replay(controller, "/");
    applied(controller, app::InsertDateTime{});
    const auto searching = controller.frame();
    expect(searching.command_line.has_value() && searching.command_line.value().text == placed &&
               vim_body(searching) == "ab\n" && editing.wall_clock().calls() == 2,
           "F5 with the search line open types the date and time into the line");
}

// 決定 4: 使う人の操作ではない合図は時刻を読まない。
void verify_quiet_signals()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::InsertText{"typed"});
    applied(controller, app::AutosaveTick{});
    applied(controller, app::WorkCompleted{});
    expect(editing.wall_clock().calls() == 0,
           "typing, the autosave tick and the work signal never read the clock");
}
} // namespace

void verify_clock_contracts()
{
    verify_text_form();
    verify_ordinary_insert();
    verify_vim();
    verify_vim_waiting();
    verify_input_line();
    verify_quiet_signals();
}
} // namespace nenenib::tests
