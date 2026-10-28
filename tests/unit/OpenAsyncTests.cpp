// scope `--open-async` の単体テスト（ADR 0042 決定 2・ADR 0159 の決定 1〜6・9・10）。開く道の読みは
// 型のある port（DocumentReadPort）を通り、同期に揃った小さなファイルは今の開く道と同じ結果、
// 裏へ回った大きなファイルは揃うまでタブを足さずステータスバーに 1 行だけ出し、今の券の結果だけを
// 採る。失敗は値で、告げ方は呼び手ごとの今の規則。まだ読んでいないタブ・起動の復元・一覧から
// 開く道も同じ port を通る。
#include "CommandText.hpp"
#include "DocumentByteLimit.hpp"
#include "DocumentReadFinished.hpp"
#include "DocumentReadProgress.hpp"
#include "DocumentReadRequest.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EndSession.hpp"
#include "FileFailure.hpp"
#include "FileHistory.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedDocumentReads.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSession.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;
using app::FileFailure;

// 裏へ回す境目。これより長い本文の要求だけが値なしで返る。
constexpr std::size_t deferred_bytes = 4;

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] std::string notice_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("-");
}

[[nodiscard]] std::string first_line(const EditorFrame &frame)
{
    return frame.lines.empty() ? std::string("-") : frame.lines.front().text;
}

[[nodiscard]] std::uint64_t last_ticket(Editing &editing)
{
    return editing.reads().requests().back().ticket;
}

[[nodiscard]] bool cancelled(Editing &editing, std::uint64_t ticket)
{
    return std::ranges::find(editing.reads().cancels(), ticket) != editing.reads().cancels().end();
}

// a.txt（小さい・同期に揃う）と big.txt・huge.txt（裏へ回る）を置き、a.txt を開いておく。
void open_small_then_defer(Editing &editing)
{
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    editing.files().hold_at("C:\\work\\big.txt", Bytes{"large body"});
    editing.files().hold_at("C:\\work\\huge.txt", Bytes{"huge body"});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{path_of("C:\\work\\a.txt")});
    editing.reads().defer_above(deferred_bytes);
}

// 小さなファイルは同期に揃い、今の開く道と同じ結果（無題に開く → 新しいタブ）。上限は core の
// 1 か所の値を要求に載せる。読み込み中の 1 行は出ない。
void verify_open_synchronous()
{
    Editing editing;
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"alpha"});
    editing.files().hold_at("C:\\work\\b.txt", Bytes{"beta"});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    const auto first = controller.apply_frame(app::OpenDocument{path_of("C:\\work\\a.txt")});
    const auto &request = editing.reads().requests().back();
    expect(editing.reads().requests().size() == 1 && request.path.text() == "C:\\work\\a.txt" &&
               request.maximum_bytes == core::maximum_document_bytes,
           "opening asks the read port once with the core limit");
    expect(first.tabs.size() == 1 && first_line(first) == "alpha" && notice_of(first) == "-",
           "a small file opens into the blank untitled without a reading line");
    const auto second = controller.apply_frame(app::OpenDocument{path_of("C:\\work\\b.txt")});
    expect(second.tabs.size() == 2 && second.document.title.text() == "b.txt" &&
               first_line(second) == "beta",
           "the next small file opens in a new tab");
    const auto again = controller.apply_frame(app::OpenDocument{path_of("C:\\work\\a.txt")});
    expect(again.tabs.size() == 2 && first_line(again) == "alpha" &&
               editing.reads().requests().size() == 2,
           "an open file switches to its tab without asking the port again");
}

// 大きなファイルは揃うまでタブを足さず、進み具合の % が進み、揃ったらタブが 1 つ足されて
// 1 行が消える。読み込み中も今のタブで打てる。
void verify_open_deferred()
{
    Editing editing;
    open_small_then_defer(editing);
    auto &controller = editing.controller();
    const auto asked = controller.apply_frame(app::OpenDocument{path_of("C:\\work\\big.txt")});
    expect(asked.tabs.size() == 1 && notice_of(asked) == "読み込み中: big.txt",
           "a deferred file shows one reading line before any progress");
    const std::uint64_t ticket = last_ticket(editing);
    editing.reads().serve(app::DocumentReadProgress{ticket, 3, 10});
    const auto partial = controller.apply_frame(app::WorkCompleted{});
    expect(partial.tabs.size() == 1 && notice_of(partial) == "読み込み中: big.txt 30%",
           "progress shows the percentage without adding a tab");
    const auto typed = controller.apply_frame(app::InsertText{"x"});
    expect(typed.tabs.size() == 1 && first_line(typed) == "xa" &&
               notice_of(typed) == "読み込み中: big.txt 30%",
           "typing works while reading and the line stays");
    editing.reads().serve(app::DocumentReadProgress{ticket, 7, 10});
    expect(notice_of(controller.apply_frame(app::WorkCompleted{})) == "読み込み中: big.txt 70%",
           "the percentage goes forward");
    editing.reads().finish(ticket);
    const auto opened = controller.apply_frame(app::WorkCompleted{});
    expect(opened.tabs.size() == 2 && opened.document.title.text() == "big.txt" &&
               first_line(opened) == "large body" && notice_of(opened) == "-",
           "the finished read adds one tab and the reading line goes away");
}

// 新しい開く意図は前の要求を取り消し、前の券の結果は捨てる。同じファイルを読み込み中にもう一度
// 開いても 2 本目を頼まない。
void verify_open_stale()
{
    Editing editing;
    open_small_then_defer(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of("C:\\work\\big.txt")});
    const std::uint64_t stale = last_ticket(editing);
    applied(controller, app::OpenDocument{path_of("C:\\work\\huge.txt")});
    const std::uint64_t fresh = last_ticket(editing);
    expect(stale != fresh && cancelled(editing, stale) && !cancelled(editing, fresh),
           "a new open cancels the previous ticket");
    const std::size_t requests = editing.reads().requests().size();
    const auto same = controller.apply_frame(app::OpenDocument{path_of("C:\\work\\huge.txt")});
    expect(editing.reads().requests().size() == requests && !cancelled(editing, fresh) &&
               notice_of(same) == "読み込み中: huge.txt",
           "opening the file being read again keeps the current request");
    editing.reads().finish(stale);
    const auto ignored = controller.apply_frame(app::WorkCompleted{});
    expect(ignored.tabs.size() == 1 && notice_of(ignored) == "読み込み中: huge.txt",
           "a stale ticket's result is dropped");
    editing.reads().finish(fresh);
    const auto opened = controller.apply_frame(app::WorkCompleted{});
    expect(opened.tabs.size() == 2 && first_line(opened) == "huge body",
           "the current ticket's result opens");
}

// 窓が閉じていくときは読み込み中の要求を取り消し、後から届いた結果ではタブを足さない。
void verify_open_end_session()
{
    Editing editing;
    open_small_then_defer(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of("C:\\work\\big.txt")});
    const std::uint64_t ticket = last_ticket(editing);
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    expect(cancelled(editing, ticket), "ending the session cancels the reading");
    editing.reads().finish(ticket);
    const auto after = controller.apply_frame(app::WorkCompleted{});
    expect(after.tabs.size() == 1 && notice_of(after) == "-",
           "a result after the session ended opens nothing");
}

constexpr std::array<FileFailure, 4> failures{FileFailure::not_found, FileFailure::too_large,
                                              FileFailure::undecodable,
                                              FileFailure::insufficient_memory};

// Ctrl+O と起動引数の道（ダイアログ）。裏で終わった失敗もタブを足さずダイアログの値に載る。
void verify_open_failure_dialog(FileFailure failure)
{
    Editing editing;
    open_small_then_defer(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of("C:\\work\\big.txt")});
    editing.reads().serve(
        app::DocumentReadFinished{last_ticket(editing), std::unexpected(failure)});
    const auto failed = controller.apply_frame(app::WorkCompleted{});
    expect(failed.tabs.size() == 1 && failed.document.last_failure == failure &&
               notice_of(failed) == "-" && first_line(failed) == "a",
           "a deferred open failure goes to the dialog and adds no tab");
}

// 一覧の履歴の候補の道（1 行の知らせ）。無くなっていたファイルだけ履歴から外す（D30）。
void verify_open_failure_notice(FileFailure failure)
{
    Editing editing;
    editing.history().serve(app::FileHistory{{path_of("C:\\work\\big.txt")}});
    open_small_then_defer(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"@"});
    const auto asked = controller.apply_frame(app::SubmitCommand{});
    expect(asked.tabs.size() == 1 && notice_of(asked) == "読み込み中: big.txt",
           "a listed file is read through the same port");
    const std::size_t writes = editing.history().writes();
    editing.reads().serve(
        app::DocumentReadFinished{last_ticket(editing), std::unexpected(failure)});
    const auto failed = controller.apply_frame(app::WorkCompleted{});
    const std::size_t forgotten = failure == FileFailure::not_found ? 1 : 0;
    expect(failed.tabs.size() == 1 && !failed.document.last_failure.has_value() &&
               notice_of(failed) == "開けませんでした: big.txt" &&
               editing.history().writes() == writes + forgotten,
           "a deferred listed failure leaves one line and forgets only a missing file");
}

void verify_open_failures()
{
    for (const FileFailure failure : failures)
    {
        verify_open_failure_dialog(failure);
        verify_open_failure_notice(failure);
    }
}

[[nodiscard]] SessionReading two_listed()
{
    const auto tab = [](std::string_view path, std::size_t rank)
    {
        return app::SessionTab{path_of(path),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    return SessionReading{std::optional<app::Session>{
        app::Session{{tab("C:\\work\\a.txt", 1), tab("C:\\work\\b.txt", 0)}, 1}}};
}

void hold_listed(ScriptedFiles &files)
{
    files.hold_at("C:\\work\\a.txt", Bytes{"a"});
    files.hold_at("C:\\work\\b.txt", Bytes{"b"});
}

// 起動の復元とまだ読んでいないタブも同じ port を通る。起動で読むのは見ていたタブだけ。
// まだ読んでいないタブが裏へ回されたら、頼みを取り消して読めなかったタブとして外す（暫定）。
void verify_open_restored()
{
    Editing restored(two_listed(), hold_listed);
    auto &controller = restored.controller();
    expect(restored.reads().requests().size() == 1 &&
               restored.reads().requests().back().path.text() == "C:\\work\\b.txt",
           "restoring the last tabs reads the active one through the port");
    applied(controller, app::VisibleLines{10});
    const auto reached = controller.apply_frame(app::SwitchTab{0});
    expect(restored.reads().requests().size() == 2 &&
               restored.reads().requests().back().path.text() == "C:\\work\\a.txt" &&
               first_line(reached) == "a",
           "a tab not read yet is read through the port when it is shown");
    Editing deferred(two_listed(), hold_listed);
    deferred.reads().defer_above(0);
    const auto dropped = deferred.controller().apply_frame(app::SwitchTab{0});
    expect(cancelled(deferred, last_ticket(deferred)) && dropped.tabs.size() == 1 &&
               notice_of(dropped) == "開けませんでした: a.txt",
           "a tab whose read is deferred is cancelled and dropped with one line");
}

// 一覧から開く道（同期）も同じ port で、揃えばタブが足される。
void verify_open_listed()
{
    Editing editing;
    editing.history().serve(app::FileHistory{{path_of("C:\\docs\\h.txt")}});
    editing.files().hold_at("C:\\docs\\h.txt", Bytes{"history"});
    open_small_then_defer(editing);
    editing.reads().defer_above(1024);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"@"});
    const auto opened = controller.apply_frame(app::SubmitCommand{});
    expect(editing.reads().requests().back().path.text() == "C:\\docs\\h.txt" &&
               opened.tabs.size() == 2 && first_line(opened) == "history",
           "a listed file opens through the port");
}
} // namespace

void verify_open_async_contracts()
{
    verify_open_synchronous();
    verify_open_deferred();
    verify_open_stale();
    verify_open_end_session();
    verify_open_failures();
    verify_open_restored();
    verify_open_listed();
}

void verify_open_async_scope()
{
    verify_open_async_contracts();
}
} // namespace nenenib::tests
