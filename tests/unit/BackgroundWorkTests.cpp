// scope `--background-work` の単体テスト（ADR 0042 決定 2・ADR 0062 の決定 7・10）。
// 裏の仕事の合図 WorkCompleted が使う人の操作の途中の状態を 1 つも動かさないことと、#271 の
// 時点で controller が同じフォルダの口（list・collect）を呼ばないこと。
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
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
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

#include <format>
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

// WorkCompleted を 1 つ送り、前後の表示値が同じで、同じフォルダの口が呼ばれないことを見る。
void expect_unmoved(Editing &editing, const char *description)
{
    const std::string before = printed(editing.controller().frame());
    const std::string after = printed(editing.controller().apply(WorkCompleted{}));
    expect(before == after && quiet(editing.folders()), description);
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
           expected == actual && quiet(worked.folders());
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

// #271 では、起動・開く・切り替え・保存・打鍵・面を開く・面の中の打鍵・窓を閉じる、のどれでも
// 同じフォルダの口を呼ばない（呼ぶのは #272 の面を開く意図と WorkCompleted）。
void verify_folder_quiet_paths()
{
    Editing editing;
    auto &controller = editing.controller();
    expect(quiet(editing.folders()), "starting never lists a folder");
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    applied(controller, app::VisibleLines{10});
    applied(controller, app::OpenDocument{core::FilePath::parse("C:\\work\\a.txt").value()});
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    applied(controller, app::InsertText{"typed"});
    applied(controller, app::SaveDocument{core::FilePath::parse("C:\\work\\a.txt").value(),
                                          core::TextEncoding::utf8});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{"a"});
    applied(controller, app::EditCommand{core::CommandEdit::backspace});
    applied(controller, app::SubmitCommand{});
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    expect(quiet(editing.folders()),
           "opening, switching, saving, typing, the palette and closing never touch the folder");
}
} // namespace

void verify_background_work_contracts()
{
    verify_work_keeps_idle_and_notice();
    verify_work_keeps_tab_walk();
    verify_work_keeps_palette_and_composition();
    verify_work_keeps_vim_state();
    verify_folder_quiet_paths();
}

void verify_background_work_scope()
{
    verify_background_work_contracts();
}
} // namespace nenenib::tests
