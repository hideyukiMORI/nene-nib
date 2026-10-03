// scope `--background-work` の単体テスト（ADR 0042 決定 2・ADR 0062 の決定 7・10）。
// 裏の仕事の合図 WorkCompleted が使う人の操作の途中の状態を 1 つも動かさないことと、同じフォルダの
// 口（list・collect）を呼ぶのが面を開く意図と合図だけであること、届いた分を面の後ろへ足すこと
// （ADR 0062 の決定 14〜18）。
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CommandChoice.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "ComposeText.hpp"
#include "CompositionView.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EndSession.hpp"
#include "FileHistory.hpp"
#include "FilePath.hpp"
#include "FolderBatch.hpp"
#include "FolderProgress.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenTabList.hpp"
#include "PaletteOrigin.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFolders.hpp"
#include "SelectEditMode.hpp"
#include "SessionEnd.hpp"
#include "SettleRecentTab.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
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
using app::WorkCompleted;

constexpr std::string_view vim_text = "one two three four";

[[nodiscard]] std::string optional_text(const std::optional<core::DisplayText> &text)
{
    return text.has_value() ? std::string(text.value().text()) : std::string("-");
}

[[nodiscard]] std::string composition_text(const std::optional<app::CompositionView> &view)
{
    return view.has_value() ? std::format("{}@{}", view.value().utf8, view.value().cursor.value)
                            : std::string("-");
}

// 本文とキャレットと選択（見えている行・行ごとの選択と検索の一致）。
[[nodiscard]] std::string printed_body(const EditorFrame &frame)
{
    std::string text =
        std::format("caret {}:{}/{} first {} total {};", frame.caret.position.line.value,
                    frame.caret.position.column.value, static_cast<int>(frame.caret.shape),
                    frame.first_visible.value, frame.total_lines);
    for (const auto &line : frame.lines)
    {
        text += std::format("{}:{}[{} {}-{}]{}{};", line.number.value, line.text,
                            static_cast<int>(line.selection.presence), line.selection.begin.value,
                            line.selection.end.value, line.matches.size(),
                            line.current_match.has_value() ? "*" : "");
    }
    return text;
}

// モード・題名・ステータスバー・入力行・知らせ・変換・録画。
[[nodiscard]] std::string printed_status(const EditorFrame &frame)
{
    std::string text =
        std::format("mode {} {} {} {} rec {} title {} saved {};", static_cast<int>(frame.mode),
                    static_cast<int>(frame.vim_mode), static_cast<int>(frame.ime), frame.mode_label,
                    frame.recording.value_or('-'), frame.document.title.text(),
                    static_cast<int>(frame.document.save_state));
    for (const auto &item : frame.status_items)
    {
        text += std::string(item.text()) + "|";
    }
    if (frame.command_line.has_value())
    {
        const auto &line = frame.command_line.value();
        text += std::format("line {} {}@{} {} {};", static_cast<int>(line.prompt), line.text,
                            line.caret.value, line.completions.size(),
                            line.completion_index.value_or(0));
    }
    return text + std::format("message {} composing {} {};", optional_text(frame.command_message),
                              composition_text(frame.composition),
                              composition_text(frame.command_composition));
}

// 面の欄（rows first selected total hint）とタブの帯。
[[nodiscard]] std::string printed_band(const EditorFrame &frame)
{
    std::string text;
    if (frame.command_palette.has_value())
    {
        const auto &palette = frame.command_palette.value();
        text += std::format("palette {} {} {} {};", palette.first, palette.selected, palette.total,
                            optional_text(palette.hint));
        for (const core::CommandChoice &row : palette.rows)
        {
            text += row.command + "|";
        }
    }
    for (const auto &tab : frame.tabs)
    {
        text += std::string(tab.title.text()) + "|";
    }
    return text + std::format("active {} scroll {} hovered {}", frame.active_tab, frame.tab_scroll,
                              frame.hovered.has_value());
}

// 表示値の比べる欄を 1 本の文字列にする（製品のコードに比較演算子を足さない・CPP-003）。
[[nodiscard]] std::string printed(const EditorFrame &frame)
{
    return printed_body(frame) + printed_status(frame) + printed_band(frame);
}

[[nodiscard]] bool quiet(const ScriptedFolders &folders)
{
    return folders.requests().empty() && folders.collects() == 0;
}

// WorkCompleted を 1 つ送り、前後の表示値が同じで、合図 1 回につき collect が 1 回・list が 0 回
// であることを見る（ADR 0062 の決定 15）。届いた分が無いので、面が開いていても何も変わらない。
void expect_unmoved(Editing &editing, const char *description)
{
    const std::string before = printed(editing.controller().frame());
    const auto requests = editing.folders().requests().size();
    const auto collects = editing.folders().collects();
    const std::string after = printed(editing.controller().apply(WorkCompleted{}));
    expect(before == after && editing.folders().requests().size() == requests &&
               editing.folders().collects() == collects + 1,
           description);
}

// Vim の鍵を before と after に分けて打ち、間に WorkCompleted を挟んだ結果が、挟まずに続けて
// 打った結果と同じか（本文の全体と表示値）。
[[nodiscard]] bool same_with_work(std::string_view before, std::string_view after)
{
    Editing plain;
    open_vim_document(plain, std::string(vim_text));
    vim_replay(plain.controller(), std::string(before) + std::string(after));
    Editing worked;
    open_vim_document(worked, std::string(vim_text));
    vim_replay(worked.controller(), before);
    expect_unmoved(worked, "a Vim key waiting for the next keeps the frame");
    vim_replay(worked.controller(), after);
    const std::string expected = printed(plain.controller().frame());
    const std::string actual = printed(worked.controller().frame());
    return whole_vim_body(plain.controller()) == whole_vim_body(worked.controller()) &&
           expected == actual && worked.folders().requests().empty() &&
           worked.folders().collects() == 1;
}

// 通常モードで a と b を打って Ctrl+Z を 1 回。work なら a と b の間に合図を挟む。
void type_and_undo(Editing &editing, bool work)
{
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::InsertText{"a"});
    if (work)
    {
        expect_unmoved(editing, "the signal between typed letters keeps the frame");
    }
    applied(editing.controller(), app::InsertText{"b"});
    applied(editing.controller(), app::HistoryAction{core::HistoryDirection::undo});
}

// 通常モードで何もしていないとき・知らせが出ているとき（決定 7）。
void verify_work_keeps_idle_and_notice()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"abc"});
    expect_unmoved(editing, "the signal changes nothing in an idle ordinary window");
    editing.history().serve(
        app::FileHistory{{core::FilePath::parse("C:\\docs\\gone.txt").value()}});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"@"});
    const auto failed = controller.apply(app::SubmitCommand{});
    expect(failed.command_message.has_value(), "a missing history file leaves one notice line");
    expect_unmoved(editing, "the signal keeps the notice line");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    const auto rejected = run_ex(controller, "tabnext 9");
    expect(rejected.command_message.has_value(), "a failed Ex command leaves its message");
    expect_unmoved(editing, "the signal keeps the Vim message");
}

// Ctrl+Tab の歩きの途中に届いても歩きは続き、次の 1 歩は使った順の 3 つ目へ行く（ADR 0058）。
void verify_work_keeps_tab_walk()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    for (const std::string_view body : {"b", "c", "d"})
    {
        applied(controller, app::NewTab{});
        applied(controller, app::InsertText{std::string(body)});
    }
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    expect_unmoved(editing, "the signal during the walk keeps the frame");
    expect(controller.tab_walking(), "the signal does not settle the walk");
    const auto third = controller.apply(app::WalkRecentTab{core::TabStep::next});
    expect(!third.lines.empty() && third.lines.front().text == "b" && controller.tab_walking(),
           "the next walk after the signal reaches the third tab in the order");
    applied(controller, app::SettleRecentTab{});
}

// 面が開いて入力と選択があるとき・面の入力行が変換中のとき・本文が変換中のとき（ADR 0061）。
void verify_work_keeps_palette_and_composition()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::NewTab{});
    applied(controller, app::NewTab{});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"#"});
    applied(controller, app::EditCommand{core::CommandEdit::complete_next});
    expect(controller.frame().command_palette.value_or(app::CommandPaletteView{}).selected == 1,
           "the palette has an input and a moved selection");
    expect_unmoved(editing, "the signal keeps the palette input, rows and selection");
    applied(controller, app::ComposeText{composed_of("あ", {}, 0)});
    expect(controller.frame().command_composition.has_value(), "the palette input is composing");
    expect_unmoved(editing, "the signal keeps the palette composition");
    Editing body;
    applied(body.controller(), app::VisibleLines{10});
    applied(body.controller(), app::InsertText{"x"});
    applied(body.controller(), app::ComposeText{composed_of("あい", {}, 3)});
    expect(body.controller().frame().composition.has_value(), "the body is composing");
    expect_unmoved(body, "the signal keeps the body composition");
}

// Vim の待ちの状態（オペレータ・回数・レジスタの接頭辞）・マクロの録画・`.` の記録・undo の単位。
void verify_work_keeps_vim_state()
{
    expect(same_with_work("d", "w"), "an operator waiting for a motion goes on after the signal");
    expect(same_with_work("2", "dw"), "a count goes on after the signal");
    expect(same_with_work("\"a", "x\"ap"), "a register prefix goes on after the signal");
    expect(same_with_work("qa", "xq@a"), "the signal never enters a macro being recorded");
    expect(same_with_work("x", "."), "the signal never replaces the last change for dot");
    expect(same_with_work("x", "u"), "undo after the signal undoes the same change");
    expect(same_with_work("ia", "b<Esc>u"), "the signal never splits an insert's undo unit");
    Editing plain;
    type_and_undo(plain, false);
    Editing worked;
    type_and_undo(worked, true);
    expect(printed(plain.controller().frame()) == printed(worked.controller().frame()),
           "the signal never splits the ordinary typing undo unit");
}

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

// 同じフォルダの口が呼ばれた回数（list・collect）。
[[nodiscard]] bool counted(ScriptedFolders &folders, std::size_t lists, std::size_t collects)
{
    return folders.requests().size() == lists && folders.collects() == collects;
}

// 起動・開く・切り替え・保存・打鍵・面の中の打鍵と ↑↓・面を閉じる・面の確定・窓を閉じる、では
// 同じフォルダの口を呼ばない。呼ぶのは面を開く 2 つの意図（collect 1 回と list 1 回・無題なら
// collect だけ）と WorkCompleted（collect 1 回）だけ（ADR 0062 の決定 14・15）。
void verify_folder_quiet_paths()
{
    Editing editing;
    auto &controller = editing.controller();
    auto &folders = editing.folders();
    expect(quiet(folders), "starting never lists a folder");
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    applied(controller, app::VisibleLines{10});
    applied(controller, app::OpenDocument{path_of("C:\\work\\a.txt")});
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    applied(controller, app::InsertText{"typed"});
    applied(controller, app::SaveDocument{path_of("C:\\work\\a.txt"), core::TextEncoding::utf8});
    expect(quiet(folders), "opening, switching, saving and typing never touch the folder");
    applied(controller, app::OpenCommandPalette{});
    expect(counted(folders, 1, 1), "opening the palette collects once and lists once");
    expect(std::string(folders.requests().back().folder.text()) == "C:\\work",
           "the palette lists the folder of the active file");
    const auto first = folders.requests().back().ticket;
    applied(controller, app::CommandText{"a"});
    applied(controller, app::EditCommand{core::CommandEdit::backspace});
    applied(controller, app::EditCommand{core::CommandEdit::complete_next});
    applied(controller, app::EditCommand{core::CommandEdit::complete_previous});
    applied(controller, app::CancelCommand{});
    expect(counted(folders, 1, 1), "typing, moving and closing in the palette stay quiet");
    applied(controller, app::OpenTabList{});
    expect(counted(folders, 2, 2) && folders.requests().back().ticket == first + 1,
           "the tab list opens the same way with the next ticket");
    applied(controller, app::OpenTabList{});
    expect(counted(folders, 2, 2), "the second tab list intent only closes the palette");
    applied(controller, app::SwitchTab{1});
    applied(controller, app::OpenCommandPalette{});
    expect(counted(folders, 2, 3), "an untitled tab collects but never lists");
    applied(controller, app::SubmitCommand{});
    applied(controller, app::ComposeText{composed_of("あ", {}, 0)});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CancelComposition{});
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    expect(counted(folders, 2, 3), "the submit, a composition and closing the window stay quiet");
    applied(controller, WorkCompleted{});
    expect(counted(folders, 2, 4), "each signal collects once and never lists");
}

// C:\work\a.txt を開いた窓。履歴は先に仕込む（面を開くときに読む）。
void open_work_file(Editing &editing)
{
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{path_of("C:\\work\\a.txt")});
}

[[nodiscard]] app::FolderBatch batch_of(std::uint64_t ticket,
                                        std::initializer_list<std::string_view> files,
                                        app::FolderProgress progress)
{
    app::FolderBatch batch{ticket, {}, progress};
    for (const std::string_view file : files)
    {
        batch.files.push_back(path_of(file));
    }
    return batch;
}

// 今の券で batch を 1 つ仕込み、合図を送る。
EditorFrame deliver(Editing &editing, std::initializer_list<std::string_view> files,
                    app::FolderProgress progress)
{
    editing.folders().serve(batch_of(editing.folders().requests().back().ticket, files, progress));
    return editing.controller().apply(WorkCompleted{});
}

// 面の行の窓（確定の文字列と出どころの補足）。
[[nodiscard]] std::string rows_of(const EditorFrame &frame)
{
    std::string text;
    for (const core::CommandChoice &row :
         frame.command_palette.value_or(app::CommandPaletteView{}).rows)
    {
        text += row.command;
        text += row.origin.has_value()
                    ? "<" + std::string(core::palette_origin_label(row.origin.value())) + ">"
                    : std::string("<>");
        text += "|";
    }
    return text;
}

[[nodiscard]] std::string selected_command(const EditorFrame &frame)
{
    const auto view = frame.command_palette.value_or(app::CommandPaletteView{});
    if (view.selected < view.first || view.selected - view.first >= view.rows.size())
    {
        return "-";
    }
    return view.rows.at(view.selected - view.first).command;
}

// 届いた分は一覧の後ろ（タブ → 履歴 → 同じフォルダ）。出さない拡張子・開いているタブと同じ
// ファイル・履歴と同じファイル（大文字と小文字だけが違うパスも）は出ず、別のフォルダにある同じ
// 名前の履歴は重なりにしない。`/` で絞ると同じフォルダだけ（ADR 0062 の決定 15）。
void verify_folder_tail()
{
    Editing editing;
    editing.history().serve(
        app::FileHistory{{path_of("C:\\work\\h.txt"), path_of("C:\\other\\n.txt")}});
    editing.files().treat_as_same("C:\\work\\h.txt", "C:\\work\\H.TXT");
    editing.files().treat_as_same("C:\\work\\a.txt", "C:\\WORK\\A.txt");
    open_work_file(editing);
    applied(editing.controller(), app::OpenCommandPalette{});
    const auto frame = deliver(editing,
                               {"C:\\WORK\\A.txt", "C:\\work\\H.TXT", "C:\\work\\n.txt",
                                "C:\\work\\pic.png", "C:\\work\\z.md", "C:\\work\\.png"},
                               app::FolderProgress::complete);
    expect(rows_of(frame) ==
               "C:\\work\\a.txt<開いているタブ>|C:\\work\\h.txt<履歴>|C:\\other\\n.txt<履歴>|"
               "C:\\work\\n.txt<同じフォルダ>|C:\\work\\z.md<同じフォルダ>|"
               "C:\\work\\.png<同じフォルダ>|",
           "the folder files follow the tabs and the history without the shown and hidden ones");
    expect(frame.command_palette.value_or(app::CommandPaletteView{}).selected == 0 &&
               !frame.command_message.has_value(),
           "a delivery keeps the first row selected and leaves no notice");
    const auto folder = editing.controller().apply(app::CommandText{"/"});
    expect(rows_of(folder) == "C:\\work\\n.txt<同じフォルダ>|C:\\work\\z.md<同じフォルダ>|C:"
                              "\\work\\.png<同じフォルダ>|",
           "the slash shows only the same folder");
}

// 古い券の batch と、面が閉じた後に届いた batch は捨てる（開き直した面に前の分が混ざらない）。
void verify_folder_stale()
{
    Editing editing;
    open_work_file(editing);
    auto &controller = editing.controller();
    auto &folders = editing.folders();
    applied(controller, app::OpenCommandPalette{});
    const auto first = folders.requests().back().ticket;
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenCommandPalette{});
    const auto opened = printed(controller.frame());
    folders.serve(batch_of(first, {"C:\\work\\old.txt"}, app::FolderProgress::complete));
    expect(printed(controller.apply(WorkCompleted{})) == opened,
           "a batch of an older ticket is dropped");
    folders.serve(batch_of(folders.requests().back().ticket, {"C:\\work\\late.txt"},
                           app::FolderProgress::complete));
    applied(controller, app::CancelCommand{});
    applied(controller, WorkCompleted{});
    applied(controller, app::OpenCommandPalette{});
    expect(rows_of(controller.frame()) == "C:\\work\\a.txt<開いているタブ>|",
           "a batch delivered after the palette closed never reaches the reopened palette");
    // 残っていた分は、次の面の券に合っていても、面を開くときの collect で捨てる。
    folders.serve(batch_of(folders.requests().back().ticket + 1, {"C:\\work\\left.txt"},
                           app::FolderProgress::complete));
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, WorkCompleted{});
    expect(rows_of(controller.frame()) == "C:\\work\\a.txt<開いているタブ>|",
           "a batch left over from the previous palette is collected away on opening");
}

// 行を選んだ後に届いても選んでいる候補は同じ。入力に文字があって届いた候補が前に割り込むときも
// （ADR 0062 の決定 16）。
void verify_folder_keeps_choice()
{
    Editing editing;
    editing.history().serve(app::FileHistory{{path_of("C:\\other\\long_q_name.txt")}});
    open_work_file(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::EditCommand{core::CommandEdit::complete_next});
    const auto moved = deliver(editing, {"C:\\work\\b.txt"}, app::FolderProgress::more);
    expect(selected_command(moved) == "C:\\other\\long_q_name.txt",
           "an empty input keeps the chosen row when the folder arrives");
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"q"});
    const auto typed = controller.frame();
    expect(selected_command(typed) == "C:\\other\\long_q_name.txt", "the query picks the history");
    const auto cut_in = deliver(editing, {"C:\\work\\q.txt"}, app::FolderProgress::complete);
    expect(rows_of(cut_in).starts_with("C:\\work\\q.txt") &&
               selected_command(cut_in) == "C:\\other\\long_q_name.txt",
           "a better folder match cuts in before the chosen row and the choice stays");
}

// 2 つの batch に分かれても 1 回の collect でまとめて届いても、順は OS が返した順。
void verify_folder_order()
{
    Editing split;
    open_work_file(split);
    applied(split.controller(), app::OpenCommandPalette{});
    static_cast<void>(
        deliver(split, {"C:\\work\\c.txt", "C:\\work\\b.txt"}, app::FolderProgress::more));
    const auto two =
        deliver(split, {"C:\\work\\e.txt", "C:\\work\\d.txt"}, app::FolderProgress::complete);
    Editing joined;
    open_work_file(joined);
    applied(joined.controller(), app::OpenCommandPalette{});
    const auto ticket = joined.folders().requests().back().ticket;
    joined.folders().serve(
        batch_of(ticket, {"C:\\work\\c.txt", "C:\\work\\b.txt"}, app::FolderProgress::more));
    joined.folders().serve(
        batch_of(ticket, {"C:\\work\\e.txt", "C:\\work\\d.txt"}, app::FolderProgress::complete));
    const auto one = joined.controller().apply(WorkCompleted{});
    expect(rows_of(two) == "C:\\work\\a.txt<開いているタブ>|C:\\work\\c.txt<同じフォルダ>|C:"
                           "\\work\\b.txt<同じフォルダ>|"
                           "C:\\work\\e.txt<同じフォルダ>|C:\\work\\d.txt<同じフォルダ>|",
           "two deliveries keep the order the OS returned");
    expect(rows_of(one) == rows_of(two) && joined.folders().collects() == 2,
           "one collect with two batches lists the same order");
}

constexpr std::string_view truncated_notice = "同じフォルダは 5 件まで。残りは一覧に出ません";

// 打ち切りは今の面の状態で、面を開いている間は打鍵・↑↓・Backspace・`/` の後も案内に出る。状態の
// 知らせ（面の入力の上限）が出る意図ではその 1 回だけそちらで、次の意図で戻る。面を閉じると出ない
// （ADR 0062 の決定 17）。
void verify_folder_truncated_stays()
{
    Editing editing;
    open_work_file(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    static_cast<void>(deliver(editing,
                              {"C:\\work\\b.txt", "C:\\work\\c.txt", "C:\\work\\d.txt",
                               "C:\\work\\e.txt", "C:\\work\\f.txt"},
                              app::FolderProgress::truncated));
    const auto typed = controller.apply(app::CommandText{"c"});
    expect(optional_text(typed.command_message) == truncated_notice,
           "the notice stays after a typed letter");
    const auto down = controller.apply(app::EditCommand{core::CommandEdit::complete_next});
    const auto up = controller.apply(app::EditCommand{core::CommandEdit::complete_previous});
    expect(optional_text(down.command_message) == truncated_notice &&
               optional_text(up.command_message) == truncated_notice,
           "the notice stays after moving the selection");
    const auto erased = controller.apply(app::EditCommand{core::CommandEdit::backspace});
    expect(optional_text(erased.command_message) == truncated_notice,
           "the notice stays after a backspace");
    const auto slash = controller.apply(app::CommandText{"/"});
    expect(optional_text(slash.command_message) == truncated_notice &&
               rows_of(slash).starts_with("C:\\work\\b.txt<同じフォルダ>|"),
           "the notice stays when the slash shows only the same folder");
    const auto refused = controller.apply(app::CommandText{std::string(300, 'a')});
    expect(refused.command_message.has_value() &&
               optional_text(refused.command_message) != truncated_notice,
           "a state notice shows once over the truncated notice");
    const auto back = controller.apply(app::EditCommand{core::CommandEdit::complete_next});
    expect(optional_text(back.command_message) == truncated_notice,
           "the next intent shows the truncated notice again");
    const auto closed = controller.apply(app::CancelCommand{});
    expect(!closed.command_message.has_value() && !closed.command_palette.has_value(),
           "the closed palette leaves no notice anywhere");
    expect(!controller.apply(WorkCompleted{}).command_message.has_value(),
           "a signal after closing tells nothing");
}

// 開き直した面は新しい券の truncated が届くまで出ない。新しい券の more・complete・failed でも、
// 古い券の truncated でも出ない。
void verify_folder_truncated_reopened()
{
    Editing editing;
    open_work_file(editing);
    auto &controller = editing.controller();
    auto &folders = editing.folders();
    applied(controller, app::OpenCommandPalette{});
    const auto old = folders.requests().back().ticket;
    static_cast<void>(deliver(editing, {"C:\\work\\b.txt"}, app::FolderProgress::truncated));
    applied(controller, app::CancelCommand{});
    const auto reopened = controller.apply(app::OpenCommandPalette{});
    expect(!reopened.command_message.has_value(), "a reopened palette starts without the notice");
    folders.serve(batch_of(old, {"C:\\work\\c.txt"}, app::FolderProgress::truncated));
    expect(!controller.apply(WorkCompleted{}).command_message.has_value(),
           "a truncated batch of an older ticket tells nothing");
    for (const auto progress :
         {app::FolderProgress::more, app::FolderProgress::complete, app::FolderProgress::failed})
    {
        expect(!deliver(editing, {}, progress).command_message.has_value(),
               "more, complete and failed of the new ticket tell nothing");
    }
}

// truncated で案内が 1 行。数は今の券で届いたファイルの合計（除く前）。complete・more・failed は
// 知らせない。failed の空の batch では一覧が変わらない（ADR 0062 の決定 17）。
void verify_folder_truncated()
{
    Editing editing;
    open_work_file(editing);
    applied(editing.controller(), app::OpenCommandPalette{});
    const auto more = deliver(editing, {"C:\\work\\a.txt", "C:\\work\\p.png", "C:\\work\\c.txt"},
                              app::FolderProgress::more);
    expect(!more.command_message.has_value(), "a batch with more leaves no notice");
    const auto cut =
        deliver(editing, {"C:\\work\\d.txt", "C:\\work\\e.txt"}, app::FolderProgress::truncated);
    expect(optional_text(cut.command_message) == truncated_notice,
           "a truncated listing tells how many files arrived");
    expect(rows_of(cut).ends_with("C:\\work\\e.txt<同じフォルダ>|"),
           "the last batch is listed with the notice");
    Editing done;
    open_work_file(done);
    applied(done.controller(), app::OpenCommandPalette{});
    const auto complete = deliver(done, {"C:\\work\\c.txt"}, app::FolderProgress::complete);
    expect(!complete.command_message.has_value(), "a complete listing leaves no notice");
    Editing broken;
    open_work_file(broken);
    applied(broken.controller(), app::OpenCommandPalette{});
    const std::string before = printed(broken.controller().frame());
    const auto failed = deliver(broken, {}, app::FolderProgress::failed);
    expect(printed(failed) == before, "a failed empty listing changes nothing and tells nothing");
}

// `:` の面で届いた分も足し、`:` を消せば出る。`:tabs` と同じ OpenTabList の面でも頼む。
void verify_folder_commands_scope()
{
    Editing editing;
    open_work_file(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{":"});
    static_cast<void>(deliver(editing, {"C:\\work\\c.txt"}, app::FolderProgress::complete));
    const auto erased = controller.apply(app::EditCommand{core::CommandEdit::backspace});
    expect(rows_of(erased) == "C:\\work\\a.txt<開いているタブ>|C:\\work\\c.txt<同じフォルダ>|",
           "a batch delivered under the colon is listed once the colon is erased");
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenTabList{});
    const auto tabs = deliver(editing, {"C:\\work\\c.txt"}, app::FolderProgress::complete);
    applied(controller, app::EditCommand{core::CommandEdit::backspace});
    expect(rows_of(tabs) == "C:\\work\\a.txt<開いているタブ>|" &&
               rows_of(controller.frame()) ==
                   "C:\\work\\a.txt<開いているタブ>|C:\\work\\c.txt<同じフォルダ>|",
           "the tab list asks for the folder too");
}

// 同じフォルダの候補を選ぶと開く。無くなっていたら 1 行の知らせで、履歴は読み書きしない。
// 履歴の候補が無くなっていたときは今までどおり履歴から外す（ADR 0062 の決定 18）。
void verify_folder_open()
{
    Editing editing;
    editing.files().hold_at("C:\\work\\z.md", Bytes{"zed"});
    open_work_file(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    static_cast<void>(deliver(editing, {"C:\\work\\z.md"}, app::FolderProgress::complete));
    applied(controller, app::CommandText{"/"});
    const auto opened = controller.apply(app::SubmitCommand{});
    expect(opened.document.title.text() == "z.md" && opened.tabs.size() == 2 &&
               !opened.lines.empty() && opened.lines.front().text == "zed",
           "a folder row opens the file in a new tab");
    const auto reads = editing.history().reads();
    applied(controller, app::OpenCommandPalette{});
    static_cast<void>(deliver(editing, {"C:\\work\\gone.txt"}, app::FolderProgress::complete));
    applied(controller, app::CommandText{"/"});
    const auto missing = controller.apply(app::SubmitCommand{});
    expect(optional_text(missing.command_message) == "開けませんでした: gone.txt",
           "a missing folder file leaves one notice line");
    expect(editing.history().reads() == reads + 1 && editing.history().writes() == 0,
           "a missing folder file never reads or writes the history");
    Editing history;
    history.history().serve(app::FileHistory{{path_of("C:\\docs\\gone.txt")}});
    open_work_file(history);
    applied(history.controller(), app::OpenCommandPalette{});
    applied(history.controller(), app::CommandText{"@"});
    applied(history.controller(), app::SubmitCommand{});
    expect(history.history().writes() == 1, "a missing history file is still forgotten");
}
} // namespace

void verify_background_work_contracts()
{
    verify_work_keeps_idle_and_notice();
    verify_work_keeps_tab_walk();
    verify_work_keeps_palette_and_composition();
    verify_work_keeps_vim_state();
    verify_folder_quiet_paths();
    verify_folder_tail();
    verify_folder_stale();
    verify_folder_keeps_choice();
    verify_folder_order();
    verify_folder_truncated();
    verify_folder_truncated_stays();
    verify_folder_truncated_reopened();
    verify_folder_commands_scope();
    verify_folder_open();
}

void verify_background_work_scope()
{
    verify_background_work_contracts();
}
} // namespace nenenib::tests
