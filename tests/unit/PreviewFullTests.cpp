// 全面の Markdown プレビューの application の契約（ADR 0155 の改訂 15〜17・20・#1052 の N5a）。
// 3 つの見せ方の遷移・Esc の戻り先・編集の拒否と知らせ・通る鍵・錨の往復・静かな道。
#include "AutosaveTick.hpp"
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardOperation.hpp"
#include "ClosePane.hpp"
#include "Column.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertDateTime.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PaneSide.hpp"
#include "PlaceCaret.hpp"
#include "PointPreview.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewPoint.hpp"
#include "PreviewSelection.hpp"
#include "PreviewSelectionText.hpp"
#include "PreviewTick.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SetPreviewPlacement.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchPane.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TogglePreview.hpp"
#include "TogglePreviewTask.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;
using app::PreviewPlacement;

constexpr std::string_view refused = "プレビュー中は編集できません。Esc で戻る";

[[nodiscard]] std::string body_of(const EditorFrame &frame)
{
    std::string joined;
    for (const auto &line : frame.lines)
    {
        if (!joined.empty())
        {
            joined += "\n";
        }
        joined += line.text;
    }
    return joined;
}

[[nodiscard]] std::string message_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] const app::PreviewFrame *side_of(const EditorFrame &frame)
{
    return frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
}

// 窓と同じ入口（apply）で Vim の鍵を流す。全面の振り分けは apply の 1 か所にある。
EditorFrame vim_keys(app::EditorController &controller, std::string_view keys)
{
    EditorFrame last = controller.frame();
    for (const auto &key : vim_keys_of(keys))
    {
        last = controller.apply_frame(app::VimKeyPress{key});
    }
    return last;
}

// 最後の要求の本文を本物の解析器で読んで、その券の結果を置く。
void serve_parsed(Editing &editing)
{
    const auto &request = editing.markdown().requests().back();
    auto parsed = core::parse_markdown(*request.text, core::MarkdownStop::never());
    editing.markdown().serve(app::MarkdownOutcome{request.ticket, std::move(parsed)});
    applied(editing.controller(), app::WorkCompleted{});
}

void open_text(Editing &editing, std::string text, std::size_t visible = 4)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{visible});
    applied(editing.controller(), app::OpenDocument{sample_path()});
}

[[nodiscard]] std::string numbered_lines(std::size_t count)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += "line " + std::to_string(line) + "\n\n";
    }
    return text;
}

// 全面の選択（全面でない・選択が無ければ nullopt）。
[[nodiscard]] std::optional<core::PreviewSelection> selection_of(const EditorFrame &frame)
{
    return frame.full_preview.has_value() ? frame.full_preview.value().selection : std::nullopt;
}

// 全面の錨の行（全面でなければ 0）。
[[nodiscard]] std::size_t anchor_line(Editing &editing, const EditorFrame &frame)
{
    if (!frame.full_preview.has_value())
    {
        return 0;
    }
    return editing.controller().text().position_of(frame.full_preview.value().anchor).line.value;
}

// 1. off → full → Esc → off（即時に頼む・本文の枠は描かない・戻ると 1 枠）。
void verify_off_full_off()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "# title\n\ntext");
    const EditorFrame full =
        controller.apply_frame(app::SetPreviewPlacement{PreviewPlacement::full});
    expect(controller.preview_placement() == PreviewPlacement::full &&
               full.full_preview.has_value() && full.lines.empty() && !full.other.has_value() &&
               full.layout == core::SplitLayout::none,
           "full shows the preview alone and builds no body lines");
    expect(editing.markdown().requests().size() == 1 &&
               *editing.markdown().requests().back().text == "# title\n\ntext",
           "entering full from off requests the body at once");
    serve_parsed(editing);
    const auto shown = controller.frame().full_preview;
    expect(shown.has_value() && shown.value().document != nullptr,
           "the result of the current ticket shows in full");
    const EditorFrame back = controller.apply_frame(app::CancelSelection{});
    expect(controller.preview_placement() == PreviewPlacement::off &&
               !back.full_preview.has_value() && !back.other.has_value() &&
               body_of(back) == "# title\n\ntext",
           "Esc from full goes back to off and draws the body again");
}

// 2. side → full → Esc → side（PreviewView が移り、頼み直さない・配置と側も戻る）。
void verify_side_full_side()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "# title\n\ntext");
    applied(controller, app::TogglePreview{});
    serve_parsed(editing);
    const auto shown = side_of(controller.frame())->document;
    const EditorFrame full =
        controller.apply_frame(app::SetPreviewPlacement{PreviewPlacement::full});
    expect(full.full_preview.has_value() && full.full_preview.value().document == shown &&
               full.layout == core::SplitLayout::none && !full.other.has_value(),
           "full takes over the side preview and its result");
    expect(editing.markdown().requests().size() == 1,
           "moving between side and full requests nothing");
    const EditorFrame back = vim_keys(controller, "<Esc>");
    expect(controller.preview_placement() == PreviewPlacement::side &&
               back.layout == core::SplitLayout::vertical && side_of(back) != nullptr &&
               side_of(back)->document == shown,
           "Esc from full goes back to the side pane with the same result");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::off});
    expect(controller.preview_placement() == PreviewPlacement::off, "off closes the side pane");
}

// 3. `:preview full|side|off` と `:preview?`（Vim の Ex・改訂 16）。
void verify_ex_preview()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "text");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    expect(message_of(run_ex(controller, "preview?")) == "preview=off", ":preview? says off");
    expect(message_of(run_ex(controller, "preview full")) == "preview=full" &&
               controller.preview_placement() == PreviewPlacement::full,
           ":preview full shows the full preview");
    expect(message_of(run_ex(controller, "preview")) == "preview=full",
           ":preview without an argument asks");
    expect(message_of(run_ex(controller, "preview side")) == "preview=side" &&
               controller.preview_placement() == PreviewPlacement::side,
           ":preview side moves the preview to the side pane");
    expect(message_of(run_ex(controller, "preview off")) == "preview=off" &&
               controller.preview_placement() == PreviewPlacement::off,
           ":preview off closes it");
    const EditorFrame wrong = run_ex(controller, "preview wide");
    expect(controller.preview_placement() == PreviewPlacement::off &&
               message_of(wrong) != "preview=off",
           "an unknown argument is an error and changes nothing");
    applied(controller, app::CancelSelection{});
}

// 4. 編集の鍵は本文に届かず 1 行知らせ、鳴らさない（改訂 17）。通常モード。
void verify_ordinary_refusals()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "abc");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const std::size_t clock_calls = editing.wall_clock().calls();
    const app::EditorIntent edits[] = {
        app::InsertText{"x"},
        app::DeleteText{core::DeleteDirection::backward, nenenib::core::DeleteUnit::character},
        app::NewLine{},
        app::ClipboardAction{app::ClipboardOperation::paste},
        app::ClipboardAction{app::ClipboardOperation::cut},
        app::HistoryAction{core::HistoryDirection::undo},
        app::InsertDateTime{},
    };
    for (const auto &edit : edits)
    {
        const EditorFrame frame = controller.apply_frame(edit);
        expect(message_of(frame) == refused, "an edit in full says it is refused in one line");
    }
    expect(editing.bell().rings() == 0 && editing.wall_clock().calls() == clock_calls,
           "refusals never ring nor read the clock");
    const EditorFrame next = controller.apply_frame(app::PreviewTick{});
    expect(message_of(next) == refused, "a tick keeps the line");
    const EditorFrame cleared = controller.apply_frame(
        app::MoveCaret{core::CaretMotion::next_character, core::SelectionAnchoring::collapse});
    expect(message_of(cleared).empty(), "the next key clears the line");
    applied(controller, app::CancelSelection{});
    expect(body_of(controller.frame()) == "abc" &&
               controller.frame().document.save_state == core::SaveState::saved,
           "the body is untouched after the refusals");
}

// 5. Vim の変更の鍵も届かない（`i` `d` `p` `x`・INSERT へも入らない）。
void verify_vim_refusals()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "abc");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    for (const std::string_view key : {"i", "d", "p", "x", "o", "u", "/", "<C-w>"})
    {
        const EditorFrame frame = vim_keys(controller, key);
        expect(message_of(frame) == refused && frame.vim_mode == core::VimMode::normal,
               "a Vim change key in full is refused and stays in NORMAL");
    }
    vim_keys(controller, "<Esc>");
    expect(body_of(controller.frame()) == "abc" &&
               controller.frame().vim_mode == core::VimMode::normal,
           "the body is untouched after the Vim refusals");
}

// 6. スクロールは錨を本文の行で動かし、本文のキャレットと先頭行は動かさない（改訂 17）。
void verify_scroll_keys()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, numbered_lines(20));
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const auto caret = controller.frame().caret.position;
    expect(anchor_line(editing, vim_keys(controller, "j")) == 2, "j moves the anchor one line");
    expect(anchor_line(editing, vim_keys(controller, "k")) == 1, "k moves it back");
    expect(anchor_line(editing, vim_keys(controller, "<C-d>")) == 3, "Ctrl-d moves half a page");
    expect(anchor_line(editing, vim_keys(controller, "<C-f>")) == 7, "Ctrl-f moves a page");
    expect(anchor_line(editing, vim_keys(controller, "<C-b>")) == 3, "Ctrl-b moves a page back");
    expect(anchor_line(editing, vim_keys(controller, "G")) == 41, "G goes to the last line");
    expect(anchor_line(editing, vim_keys(controller, "gg")) == 1, "gg goes to the first line");
    expect(anchor_line(editing, controller.apply_frame(app::ScrollLines{3})) == 4,
           "the wheel over the body moves the anchor");
    expect(anchor_line(editing, controller.apply_frame(app::ScrollPane{-1})) == 3,
           "the wheel over the hidden pane moves the anchor too");
    const EditorFrame after = vim_keys(controller, "<Down>");
    expect(anchor_line(editing, after) == 4 && after.caret.position == caret &&
               after.first_visible.value == 1,
           "scrolling never moves the caret nor the body");
    applied(controller, app::SelectEditMode{core::EditMode::ordinary});
    const auto ordinary = [&](core::CaretMotion motion)
    {
        return anchor_line(editing, controller.apply_frame(app::MoveCaret{
                                        motion, core::SelectionAnchoring::collapse}));
    };
    expect(ordinary(core::CaretMotion::next_line) == 5, "Down moves the anchor in ordinary mode");
    expect(ordinary(core::CaretMotion::page_down) == 9, "PgDn moves a page");
    expect(ordinary(core::CaretMotion::document_start) == 1, "Ctrl+Home goes to the first line");
    expect(ordinary(core::CaretMotion::next_character) == 1 &&
               message_of(controller.frame()).empty(),
           "Right reaches the preview and does nothing");
}

// 7. 錨の往復: 全面で送った位置へ off の本文が揃い、側へ戻ると側の錨も同じ（改訂 17）。
void verify_anchor_round_trip()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, numbered_lines(20));
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    applied(controller, app::ScrollLines{10});
    const EditorFrame off = controller.apply_frame(app::CancelSelection{});
    expect(off.first_visible.value == 11, "leaving full lines the body up with the anchor");
    applied(controller, app::TogglePreview{});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame full = controller.frame();
    expect(anchor_line(editing, full) == 11, "full from side keeps the followed anchor");
    applied(controller, app::ScrollLines{-4});
    const EditorFrame side = controller.apply_frame(app::CancelSelection{});
    expect(side.first_visible.value == 7 &&
               controller.text().position_of(side_of(side)->anchor).line.value == 7,
           "back in the side pane the body and the preview show the same line");
}

// 8. 通る鍵: タブ（新しいタブ・帯の切り替え・`gt`）・Ex・チェックボックスのクリック・Space の規則。
void verify_passing_keys()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "- [ ] a\n\ntext");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    const EditorFrame clicked = controller.apply_frame(app::TogglePreviewTask{0});
    applied(controller, app::CancelSelection{});
    expect(body_of(controller.frame()) == "- [x] a\n\ntext" && message_of(clicked).empty(),
           "the checkbox click is the one edit that reaches the body");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    applied(controller, app::InsertText{" "});
    applied(controller, app::CancelSelection{});
    expect(body_of(controller.frame()) == "- [ ] a\n\ntext",
           "Space toggles the task on the caret line");
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{3}, core::Column{0}},
                                        core::SelectionAnchoring::collapse});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    expect(message_of(controller.apply_frame(app::InsertText{" "})) == "この行にタスクはありません",
           "Space on a line without a task says so");
    const std::size_t asked = editing.markdown().requests().size();
    const EditorFrame added = controller.apply_frame(app::NewTab{});
    expect(added.tabs.size() == 2 && added.full_preview.has_value() &&
               editing.markdown().requests().size() == asked + 1,
           "a new tab passes and the full preview follows the new document at once");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    const EditorFrame walked = vim_keys(controller, "gt");
    expect(walked.active_tab == 0 && walked.full_preview.has_value(),
           "gt passes to the tab destination");
    applied(controller, app::SwitchTab{1});
    expect(controller.frame().active_tab == 1, "the tab band passes");
}

// 9. 静かな道: off の間と全面から off へ戻った後は、打鍵・合図・裏の仕事で port を呼ばない
// （決定 10・改訂 19）。全面の間の tick は側と同じく打つのを止めてから頼む。
void verify_quiet_paths_full()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "text");
    applied(controller, app::InsertText{"a"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::WorkCompleted{});
    expect(editing.markdown().requests().empty() && editing.markdown().collects() == 0,
           "off never calls the Markdown port");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    applied(controller, app::CancelSelection{});
    const std::size_t asked = editing.markdown().requests().size();
    const std::size_t collected = editing.markdown().collects();
    applied(controller, app::InsertText{"b"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    applied(controller, app::WorkCompleted{});
    expect(editing.markdown().requests().size() == asked &&
               editing.markdown().collects() == collected,
           "after leaving full the port is quiet again");
}

// 10. 全面の間の `:q` `:q!` `:wq` `:x`・枠を閉じる・`:only` は全面を抜けて off にするだけ（ADR 0155
// の追補 33）。タブも隠した枠も閉じず、未保存を捨てない。`:wq` `:x` は保存してから抜ける。
void verify_quit_leaves_full()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "text");
    applied(controller, app::InsertText{"a"});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    for (const std::string_view quit : {"q", "q!"})
    {
        applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
        const EditorFrame left = run_ex(controller, std::string(quit));
        expect(controller.preview_placement() == PreviewPlacement::off && left.tabs.size() == 1 &&
                   !left.closing && body_of(left) == "atext" &&
                   left.document.save_state == core::SaveState::modified,
               ":q and :q! in full leave full and keep the tab and the unsaved body");
    }
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame written = run_ex(controller, "wq");
    expect(controller.preview_placement() == PreviewPlacement::off && written.tabs.size() == 1 &&
               !written.closing && written.document.save_state == core::SaveState::saved,
           ":wq in full saves and leaves full");
    applied(controller, app::InsertText{"b"});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame updated = run_ex(controller, "x");
    expect(controller.preview_placement() == PreviewPlacement::off && updated.tabs.size() == 1 &&
               updated.document.save_state == core::SaveState::saved,
           ":x in full saves the change and leaves full");
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame closed = controller.apply_frame(app::ClosePane{});
    expect(controller.preview_placement() == PreviewPlacement::off &&
               closed.layout == core::SplitLayout::vertical && closed.other.has_value(),
           "closing the pane in full leaves full and keeps the hidden body panes");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame only = run_ex(controller, "only");
    expect(controller.preview_placement() == PreviewPlacement::off &&
               only.layout == core::SplitLayout::vertical,
           ":only in full leaves full and keeps the hidden body panes");
}

// 11. 本文の 2 枠は全面の間も保ったまま隠し、Esc で戻る（改訂 15）。右の側から入った側の
// プレビューも、配置と側ごと戻る。全面の間も自動退避の合図は通り、知らせを出さない（R1-1）。
void verify_hidden_panes_return()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "text");
    applied(controller, app::SplitPane{core::SplitLayout::horizontal});
    applied(controller, app::SwitchPane{});
    const EditorFrame split = controller.frame();
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    applied(controller, app::InsertText{"x"});
    const EditorFrame back = controller.apply_frame(app::CancelSelection{});
    expect(split.active_side == core::PaneSide::second && back.layout == split.layout &&
               back.active_side == split.active_side && back.other.has_value() &&
               std::holds_alternative<app::PaneFrame>(back.other.value()),
           "Esc from full brings the hidden body panes back as they were");
    applied(controller, app::TogglePreview{});
    const EditorFrame side = controller.frame();
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame returned = controller.apply_frame(app::CancelSelection{});
    expect(side_of(side) != nullptr && returned.layout == side.layout &&
               returned.active_side == core::PaneSide::second && side_of(returned) != nullptr,
           "side, full and side again keep the layout and the second side");

    Editing saved;
    open_text(saved, "text");
    applied(saved.controller(), app::InsertText{"a"});
    applied(saved.controller(), app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame ticked = saved.controller().apply_frame(app::AutosaveTick{});
    expect(saved.autosave().stored().size() == 1 && message_of(ticked).empty(),
           "the autosave tick passes in full and says nothing");
}

// 12. コピーの本文は見えている文字（改訂 18）。見出し・段落・リスト・コードを要素の境の改行 1 つで
// つなぎ、Markdown の記号・リストの点と番号・チェックボックスは入れず、コードはそのまま、水平線は
// 飛ばす。向きに依らず同じで、空の選択は空。core の純関数 preview_selection_text の 1 本。
void verify_selection_text()
{
    const auto parsed = core::parse_markdown(
        "# Title\n\nsome *text* here\n\n1. one\n2. two\n\n- [ ] task\n\n---\n\n```\ncode\n  "
        "line\n```\n",
        core::MarkdownStop::never());
    if (!parsed)
    {
        expect(false, "the selection sample parses");
        return;
    }
    const core::MarkdownDocument &document = parsed.value();
    const auto all = core::preview_selection_all(document);
    expect(all.has_value() && core::preview_selection_text(document, all.value()) ==
                                  "Title\nsome text here\none\ntwo\ntask\ncode\n  line",
           "select all copies the shown text of every block joined by one newline");
    const core::PreviewSelection forward{1, 5, 2, 2};
    const core::PreviewSelection backward{2, 2, 1, 5};
    expect(core::preview_selection_text(document, forward) == "text here\non" &&
               core::preview_selection_text(document, backward) == "text here\non",
           "a partial selection copies the same text in either direction");
    expect(core::preview_selection_text(document, core::PreviewSelection{1, 3, 1, 3}).empty() &&
               !core::preview_selected_bytes(document, core::PreviewSelection{1, 3, 1, 3}, 1)
                    .has_value(),
           "an empty selection copies and paints nothing");
    const auto bytes = core::preview_selected_bytes(document, forward, 1);
    expect(bytes.has_value() && bytes.value().first == 5 &&
               bytes.value().second == std::string_view("some text here").size() &&
               !core::preview_selected_bytes(document, forward, 0).has_value(),
           "the painted bytes of a block follow the ordered selection");
    core::PreviewPoint point{0, 0};
    for (int step = 0; step < 6; ++step)
    {
        point = core::preview_point_moved(document, point, core::CaretMotion::next_character);
    }
    expect(point == core::PreviewPoint{1, 0} &&
               core::preview_point_moved(document, point, core::CaretMotion::previous_character) ==
                   core::PreviewPoint{0, 5} &&
               core::preview_point_moved(document, point, core::CaretMotion::next_line) ==
                   core::PreviewPoint{1, std::string_view("some text here").size()} &&
               core::preview_point_moved(document, core::PreviewPoint{4, 4},
                                         core::CaretMotion::next_character) ==
                   core::PreviewPoint{6, 0},
           "Shift+arrows step one character across block ends, by block up and down, and skip "
           "the rule");
}

// 13. 全面の選択とコピーの鍵（改訂 18）。Shift+矢印は錨の要素の頭から選び、Ctrl+C は文書の改行の
// 形で ClipboardPort へ置き（替え玉だけ）、Ctrl+A は全部、マウスは押した点から伸ばす。どれも本文を
// 変えず知らせも出さない。選択がある間の Space は本文に届かない鍵。全面をやめると選択は消える。
void verify_selection_keys()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "# Title\r\n\r\ntext\r\n");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    for (int step = 0; step < 3; ++step)
    {
        applied(controller, app::MoveCaret{core::CaretMotion::next_character,
                                           core::SelectionAnchoring::extend});
    }
    const EditorFrame stepped = controller.frame();
    expect(selection_of(stepped) == core::PreviewSelection{0, 0, 0, 3} &&
               message_of(stepped).empty(),
           "Shift+Right selects from the start of the anchor block");
    applied(controller, app::ClipboardAction{app::ClipboardOperation::copy});
    expect(editing.clipboard().writes() == 1 && editing.clipboard().read().value_or("") == "Tit",
           "Ctrl+C puts the selected shown text on the clipboard");
    applied(controller, app::SelectAll{});
    applied(controller, app::ClipboardAction{app::ClipboardOperation::copy});
    expect(editing.clipboard().read().value_or("") == "Title\r\ntext",
           "Ctrl+A selects every block and the copy uses the document newline");
    const EditorFrame pressed = controller.apply_frame(
        app::PointPreview{core::PreviewPoint{1, 1}, core::SelectionAnchoring::collapse});
    const EditorFrame dragged = controller.apply_frame(
        app::PointPreview{core::PreviewPoint{1, 3}, core::SelectionAnchoring::extend});
    expect(selection_of(pressed) == core::PreviewSelection{1, 1, 1, 1} &&
               selection_of(dragged) == core::PreviewSelection{1, 1, 1, 3},
           "a press starts an empty selection and a drag extends it");
    expect(message_of(controller.apply_frame(app::InsertText{" "})) == refused,
           "Space with a selection does not reach the body");
    applied(controller, app::CancelSelection{});
    expect(body_of(controller.frame()) == "# Title\n\ntext\n" &&
               controller.frame().document.save_state == core::SaveState::saved,
           "selecting and copying never change the body");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    applied(controller, app::SelectAll{});
    // 全面から側へ移すのは `:preview side`（Ctrl+Shift+M は全面から閉じる・D77）。
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::side});
    const EditorFrame side = controller.frame();
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    expect(side_of(side) != nullptr && !side_of(side)->selection.has_value() &&
               controller.frame().full_preview.has_value() &&
               !selection_of(controller.frame()).has_value(),
           "the selection does not move to the side pane nor back to full");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::off});
    applied(controller, app::SelectAll{});
    const std::size_t writes = editing.clipboard().writes();
    applied(controller,
            app::PointPreview{core::PreviewPoint{0, 2}, core::SelectionAnchoring::extend});
    applied(controller, app::ClipboardAction{app::ClipboardOperation::copy});
    expect(editing.clipboard().writes() == writes + 1U &&
               editing.clipboard().read().value_or("").starts_with("# Title"),
           "outside full the preview pointer does nothing and Ctrl+C copies the body as before");
}

// 14. Vim の全面の y と Ctrl-a（改訂 18）。y はオペレータを待たずに選択を置き、Ctrl-a は数を
// 増やさず全部を選ぶ。置けないときは 1 行知らせ、本文は変えない（ADR 0134）。
void verify_vim_selection()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "a 1\n\nb\n");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    applied(controller, app::VimKeyPress{core::VimKey{core::VimSpecialKey::control_a}});
    const EditorFrame copied = vim_keys(controller, "y");
    expect(editing.clipboard().read().value_or("") == "a 1\nb" && message_of(copied).empty(),
           "Ctrl-a selects all and y copies it in Vim");
    editing.clipboard().refuse_writes();
    const EditorFrame refused_copy = vim_keys(controller, "y");
    applied(controller, app::CancelSelection{});
    expect(!message_of(refused_copy).empty() && body_of(controller.frame()) == "a 1\n\nb\n",
           "a refused copy says so and the number was not incremented");
}

// 15. 解析の結果が差し替わると選択を捨てる（ADR 0155 の追補 決定 38）。チェックボックスの切り替えで
// 本文が変わり、新しい結果が届いた後は選択が無く、知らせも出ない。
void verify_selection_dropped_on_result()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "- [ ] a\n\ntext");
    applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    serve_parsed(editing);
    applied(controller, app::SelectAll{});
    const bool chosen = selection_of(controller.frame()).has_value();
    applied(controller, app::TogglePreviewTask{0});
    serve_parsed(editing);
    const EditorFrame replaced = controller.frame();
    expect(chosen && replaced.full_preview.has_value() && !selection_of(replaced).has_value() &&
               message_of(replaced).empty(),
           "a replaced result drops the selection and says nothing");
}

// 16〜19 の起点の見せ方（施主決定 D77・ADR 0155 の追補 #1354）。"side full" は側から全面へ入った
// 全面（Esc の戻り先が side）、"full" は off から入った全面（戻り先が off）。
void reach_placement(app::EditorController &controller, std::string_view start)
{
    if (start == "side" || start == "side full")
    {
        applied(controller, app::SetPreviewPlacement{PreviewPlacement::side});
    }
    if (start == "full" || start == "side full")
    {
        applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
    }
}

// 巡回の表の入力 1 つ。"toggle" は Ctrl+Shift+M（操作 toggle_preview）、"full operation" は F1 の
// 「Markdown プレビュー（全面）」、"esc" は全面の間の Esc、ほかは Vim の Ex。
void give_input(app::EditorController &controller, std::string_view input)
{
    if (input == "toggle")
    {
        applied(controller, app::TogglePreview{});
        return;
    }
    if (input == "full operation")
    {
        applied(controller, app::SetPreviewPlacement{PreviewPlacement::full});
        return;
    }
    if (input == "esc")
    {
        static_cast<void>(vim_keys(controller, "<Esc>"));
        return;
    }
    static_cast<void>(run_ex(controller, std::string(input)));
}

[[nodiscard]] core::FilePath markdown_path()
{
    return core::FilePath::parse("C:\\work\\note.md").value();
}

using CycleRow = std::tuple<std::string_view, std::string_view, PreviewPlacement>;

// 表の 1 行: 起点の見せ方へ行き、入力を 1 つ与えて、着いた見せ方が期待と違えばその行の名前を
// 返す（Vim モード）。合っていれば空。
[[nodiscard]] std::string cycle_row_mismatch(bool markdown, const CycleRow &row)
{
    const auto &[start, input, expected] = row;
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold(Bytes{std::string{"# title\n\ntext"}});
    applied(controller, app::VisibleLines{4});
    applied(controller, app::OpenDocument{markdown ? markdown_path() : sample_path()});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    reach_placement(controller, start);
    give_input(controller, input);
    if (controller.preview_placement() == expected)
    {
        return {};
    }
    return std::string(markdown ? "markdown: " : "text: ") + std::string(start) + " + " +
           std::string(input) + "\n";
}

// 16. 巡回の表（3 つの見せ方 × 入力）。Ctrl+Shift+M は off → side → full → off と巡り、`:preview`
// （問い）・`:preview full`・F1 の全面・全面の間の Esc は今の意味のまま。Markdown の文書でも
// Markdown でない文書（note.txt）でも同じ。
void verify_toggle_cycle_table()
{
    expect(app::cycled_preview_placement(PreviewPlacement::off) == PreviewPlacement::side &&
               app::cycled_preview_placement(PreviewPlacement::side) == PreviewPlacement::full &&
               app::cycled_preview_placement(PreviewPlacement::full) == PreviewPlacement::off,
           "the cycle is off, side, full and off again");
    constexpr std::array<CycleRow, 20> rows{{
        {"off", "toggle", PreviewPlacement::side},
        {"side", "toggle", PreviewPlacement::full},
        {"full", "toggle", PreviewPlacement::off},
        {"side full", "toggle", PreviewPlacement::off},
        {"off", "preview", PreviewPlacement::off},
        {"side", "preview", PreviewPlacement::side},
        {"full", "preview", PreviewPlacement::full},
        {"side full", "preview", PreviewPlacement::full},
        {"off", "preview full", PreviewPlacement::full},
        {"side", "preview full", PreviewPlacement::full},
        {"full", "preview full", PreviewPlacement::full},
        {"side full", "preview full", PreviewPlacement::full},
        {"off", "full operation", PreviewPlacement::full},
        {"side", "full operation", PreviewPlacement::full},
        {"full", "full operation", PreviewPlacement::full},
        {"side full", "full operation", PreviewPlacement::full},
        {"off", "esc", PreviewPlacement::off},
        {"side", "esc", PreviewPlacement::side},
        {"full", "esc", PreviewPlacement::off},
        {"side full", "esc", PreviewPlacement::side},
    }};
    std::string mismatches;
    for (const bool markdown : {true, false})
    {
        for (const CycleRow &row : rows)
        {
            mismatches += cycle_row_mismatch(markdown, row);
        }
    }
    std::fputs(mismatches.c_str(), stderr);
    expect(mismatches.empty(),
           "every row of the cycle table lands on its placement, Markdown or not");
}

// 17. 3 回押すと元に戻る。頼むのは off から側を開くときの 1 回だけで、側 → 全面 → off は券を
// 進めない。全面の間に押しても本文の先頭行は錨の行に揃う（追補 31）。通常モードでも Vim でも同じ。
void verify_toggle_round_trip()
{
    for (const auto mode : {core::EditMode::ordinary, core::EditMode::vim})
    {
        Editing editing;
        auto &controller = editing.controller();
        open_text(editing, "# title\n\ntext");
        applied(controller, app::SelectEditMode{mode});
        const EditorFrame side = controller.apply_frame(app::TogglePreview{});
        const EditorFrame full = controller.apply_frame(app::TogglePreview{});
        const EditorFrame off = controller.apply_frame(app::TogglePreview{});
        expect(side_of(side) != nullptr && !side.full_preview.has_value() &&
                   full.full_preview.has_value() && !full.other.has_value() &&
                   !off.full_preview.has_value() && !off.other.has_value() &&
                   off.layout == core::SplitLayout::none && body_of(off) == "# title\n\ntext",
               "three presses go side, full and back to one body pane");
        expect(editing.markdown().requests().size() == 1,
               "only opening the side pane requests a parse");
        applied(controller, app::TogglePreview{});
        expect(controller.preview_placement() == PreviewPlacement::side &&
                   editing.markdown().requests().size() == 2,
               "the fourth press opens the side pane again with a fresh request");
    }
}

// 18. 本文の 2 枠のとき。側を開くと本文の枠を置き換え、全面 → off で 1 枠になる。2 枠のまま
// `:preview full`（F1 の全面）で入った全面から押すと、隠した 2 枠が配置と側ごと戻る。
void verify_toggle_with_split()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "text");
    applied(controller, app::SplitPane{core::SplitLayout::vertical});
    const EditorFrame side = controller.apply_frame(app::TogglePreview{});
    const EditorFrame full = controller.apply_frame(app::TogglePreview{});
    const EditorFrame off = controller.apply_frame(app::TogglePreview{});
    expect(side.layout == core::SplitLayout::vertical && side_of(side) != nullptr &&
               full.full_preview.has_value() && off.layout == core::SplitLayout::none &&
               !off.other.has_value(),
           "with two body panes the cycle replaces the other pane and ends in one pane");

    Editing hidden;
    auto &split = hidden.controller();
    open_text(hidden, "text");
    applied(split, app::SplitPane{core::SplitLayout::horizontal});
    applied(split, app::SwitchPane{});
    applied(split, app::SetPreviewPlacement{PreviewPlacement::full});
    const EditorFrame back = split.apply_frame(app::TogglePreview{});
    expect(split.preview_placement() == PreviewPlacement::off &&
               back.layout == core::SplitLayout::horizontal &&
               back.active_side == core::PaneSide::second && back.other.has_value() &&
               std::holds_alternative<app::PaneFrame>(back.other.value()),
           "the key in full entered from two body panes brings both panes back");
}

// 19. タブを切り替えても見せ方は窓の値のまま巡る。側を開いたまま新しいタブへ行き、そこで全面へ、
// 全面の間に前のタブへ行って戻り、もう一度押すと閉じる。
void verify_toggle_across_tabs()
{
    Editing editing;
    auto &controller = editing.controller();
    open_text(editing, "# title\n\ntext");
    applied(controller, app::TogglePreview{});
    applied(controller, app::NewTab{});
    expect(controller.preview_placement() == PreviewPlacement::side,
           "the side pane stays open on a new tab");
    applied(controller, app::TogglePreview{});
    expect(controller.preview_placement() == PreviewPlacement::full,
           "the key on the new tab moves the preview to full");
    applied(controller, app::SwitchTab{0});
    applied(controller, app::SwitchTab{1});
    const EditorFrame returned = controller.frame();
    expect(returned.active_tab == 1 && returned.full_preview.has_value(),
           "going to another tab and back keeps the full preview");
    applied(controller, app::TogglePreview{});
    expect(controller.preview_placement() == PreviewPlacement::off &&
               !controller.frame().other.has_value(),
           "the next press after the tab round trip closes the preview");
}
} // namespace

void verify_preview_full_scope()
{
    verify_off_full_off();
    verify_side_full_side();
    verify_ex_preview();
    verify_ordinary_refusals();
    verify_vim_refusals();
    verify_scroll_keys();
    verify_anchor_round_trip();
    verify_passing_keys();
    verify_quiet_paths_full();
    verify_quit_leaves_full();
    verify_hidden_panes_return();
    verify_selection_text();
    verify_selection_keys();
    verify_vim_selection();
    verify_selection_dropped_on_result();
    verify_toggle_cycle_table();
    verify_toggle_round_trip();
    verify_toggle_with_split();
    verify_toggle_across_tabs();
}
} // namespace nenenib::tests
