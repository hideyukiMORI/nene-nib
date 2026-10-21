// Markdown のプレビューの application の契約（ADR 0155）。枠に置く・券・間引き・即時の要求・
// 操作中にならない枠・閉じる・置き換える・16 MiB・錨と片方向の追従・静かな道。
#include "CaretMotion.hpp"
#include "ClosePane.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MarkdownDocument.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PaneFrame.hpp"
#include "PlaceCaret.hpp"
#include "PreviewBlock.hpp"
#include "PreviewBlockKind.hpp"
#include "PreviewFrame.hpp"
#include "PreviewTick.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "SelectionAnchoring.hpp"
#include "SettleRecentTab.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SwitchPane.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "TogglePreview.hpp"
#include "VimKey.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;
using core::SplitLayout;

// もう 1 つの枠がプレビューなら、その表示値。
[[nodiscard]] const app::PreviewFrame *preview_of(const EditorFrame &frame)
{
    return frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
}

[[nodiscard]] std::string message_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] std::string notice_of(const app::PreviewFrame &frame)
{
    return frame.notice.has_value() ? std::string(frame.notice.value().text()) : std::string{};
}

// Vim の `Ctrl-w` に続けて 1 鍵。
void window_command(app::EditorController &controller, std::string_view key)
{
    static_cast<void>(controller.press_vim_key(core::VimKey{core::VimSpecialKey::control_w}));
    vim_replay(controller, key);
}

// 段落 1 つの結果（中身は券の見分けにだけ使う）。
[[nodiscard]] app::MarkdownOutcome parsed(std::uint64_t ticket, std::string_view text)
{
    core::MarkdownDocument document;
    document.blocks.push_back(core::PreviewBlock{
        core::PreviewBlockKind::paragraph, 0, 0, 0, {}, core::Offset{0}, std::string(text), {}});
    return app::MarkdownOutcome{ticket, std::move(document)};
}

[[nodiscard]] app::MarkdownOutcome failed(std::uint64_t ticket, core::MarkdownFailure failure)
{
    return app::MarkdownOutcome{ticket, std::unexpected(failure)};
}

// 見せている結果の最初のブロックの文字（結果が無ければ "<none>"）。
[[nodiscard]] std::string shown_text(const app::PreviewFrame &frame)
{
    if (frame.document == nullptr || frame.document->blocks.empty())
    {
        return "<none>";
    }
    return frame.document->blocks.front().text;
}

// 1. 開くと左右に分けてプレビューを置き、券 1 で今の本文を頼む（決定 3・5・6）。
void verify_open_requests()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"# title\nbody"});
    const EditorFrame opened = controller.apply_frame(app::TogglePreview{});
    const auto *preview = preview_of(opened);
    const auto &markdown = editing.markdown();
    expect(opened.layout == SplitLayout::vertical && preview != nullptr &&
               preview->document == nullptr && !preview->notice.has_value(),
           "TogglePreview splits left and right and puts an empty preview on the other side");
    expect(markdown.requests().size() == 1 && markdown.requests().front().ticket == 1 &&
               *markdown.requests().front().text == "# title\nbody",
           "opening requests the whole body with the first ticket");
    expect(markdown.collects() == 1, "opening drops what was left from before once");
}

// 2. 合図で今の券の結果だけを置き、古い券と打ち切りは捨てる（決定 6）。
void verify_ticket_results()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"body"});
    applied(controller, app::TogglePreview{});
    auto &markdown = editing.markdown();
    markdown.serve(parsed(0, "stale"));
    markdown.serve(parsed(1, "fresh"));
    const EditorFrame done = controller.apply_frame(app::WorkCompleted{});
    const auto *preview = preview_of(done);
    expect(preview != nullptr && shown_text(*preview) == "fresh",
           "WorkCompleted puts only the result of the current ticket");
    markdown.serve(failed(1, core::MarkdownFailure::stopped));
    const EditorFrame stopped = controller.apply_frame(app::WorkCompleted{});
    expect(shown_text(*preview_of(stopped)) == "fresh" && notice_of(*preview_of(stopped)).empty(),
           "a stopped parse is dropped and the previous result stays");
    markdown.serve(failed(1, core::MarkdownFailure::parser_rejected));
    const EditorFrame rejected = controller.apply_frame(app::WorkCompleted{});
    expect(shown_text(*preview_of(rejected)) == "fresh" &&
               notice_of(*preview_of(rejected)) == "Markdown を解析できませんでした",
           "a rejected parse keeps the previous result and adds one line");
    markdown.serve(failed(1, core::MarkdownFailure::insufficient_memory));
    const EditorFrame short_of_memory = controller.apply_frame(app::WorkCompleted{});
    expect(shown_text(*preview_of(short_of_memory)) == "fresh" &&
               notice_of(*preview_of(short_of_memory)) ==
                   "メモリが足りずプレビューできませんでした",
           "a parse out of memory keeps the previous result and adds one line");
    markdown.serve(failed(1, core::MarkdownFailure::too_large));
    expect(notice_of(*preview_of(controller.apply_frame(app::WorkCompleted{}))) ==
               "プレビューは 16 MiB までです",
           "a too large body adds the 16 MiB line");
    markdown.serve(parsed(1, "again"));
    const EditorFrame again = controller.apply_frame(app::WorkCompleted{});
    expect(shown_text(*preview_of(again)) == "again" && notice_of(*preview_of(again)).empty(),
           "a later result of the current ticket replaces the result and clears the line");
}

// 3. 打つのを止めてから 1 tick 以上たった本文だけを頼む（決定 8）。
void verify_tick_thinning()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::TogglePreview{});
    const auto &markdown = editing.markdown();
    applied(controller, app::PreviewTick{});
    expect(markdown.requests().size() == 1, "a tick without typing requests nothing");
    applied(controller, app::InsertText{"a"});
    applied(controller, app::PreviewTick{});
    expect(markdown.requests().size() == 1, "the first tick after typing only sees the revision");
    applied(controller, app::PreviewTick{});
    expect(markdown.requests().size() == 2 && markdown.requests().back().ticket == 2 &&
               *markdown.requests().back().text == "a",
           "the next tick without typing requests the body with the next ticket");
    applied(controller, app::PreviewTick{});
    expect(markdown.requests().size() == 2, "a tick after the request requests nothing");
    applied(controller, app::InsertText{"b"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::InsertText{"c"});
    applied(controller, app::PreviewTick{});
    expect(markdown.requests().size() == 2, "typing between ticks keeps thinning the requests");
}

// 3b. 静かな tick は描き直さない（ADR 0155 の追補・#813）。開いて結果を採った後の 10 回の tick は
// どれも repaint が false で、打鍵の後は改訂番号を見るだけの tick も false・頼む tick は true。
// 閉じた後の tick も false。ほかの意図は true のまま。
void verify_quiet_ticks_do_not_repaint()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    expect(controller.apply(app::TogglePreview{}).repaint, "opening the preview repaints");
    editing.markdown().serve(parsed(1, ""));
    expect(controller.apply(app::WorkCompleted{}).repaint, "taking the parse result repaints");
    bool quiet = true;
    for (int tick = 0; tick < 10; ++tick)
    {
        quiet = quiet && !controller.apply(app::PreviewTick{}).repaint;
    }
    expect(quiet && editing.markdown().requests().size() == 1,
           "ten quiet ticks over an open preview never ask for a repaint");
    expect(controller.apply(app::InsertText{"a"}).repaint, "typing repaints");
    expect(!controller.apply(app::PreviewTick{}).repaint,
           "the tick that only sees the new revision asks for no repaint");
    expect(controller.apply(app::PreviewTick{}).repaint &&
               editing.markdown().requests().size() == 2,
           "the tick that requests the typed body asks for a repaint");
    applied(controller, app::TogglePreview{});
    expect(!controller.apply(app::PreviewTick{}).repaint,
           "a tick after the preview closed asks for no repaint");
}

// 3c. プレビューが開いている間の tick は上下に動くときの狙いの横位置を捨てない（ADR 0155 の
// 追補・#813）。通常モードの下向きと Vim の `j`。
void verify_tick_keeps_wanted_column()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"abcdef\nx\nabcdef"});
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    applied(controller, app::TogglePreview{});
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{5}},
                                        core::SelectionAnchoring::collapse});
    const auto down =
        app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse};
    applied(controller, down);
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    expect(controller.apply_frame(down).caret.position ==
               core::TextPosition{core::LineNumber{3}, core::Column{5}},
           "preview ticks between two downward moves keep the wanted x");
}

// 3d. Vim の `j` も同じ。
void verify_tick_keeps_vim_wanted_column()
{
    Editing vim;
    open_vim_document(vim, "abcdef\nx\nabcdef");
    applied(vim.controller(), app::TogglePreview{});
    vim_replay(vim.controller(), "4lj");
    applied(vim.controller(), app::PreviewTick{});
    applied(vim.controller(), app::PreviewTick{});
    vim_replay(vim.controller(), "j");
    expect(caret_at(vim.controller().frame(), 3, 5),
           "preview ticks between two j keep the Vim wanted column");
}

// 4. 操作中の文書が替わると tick を待たずに頼み、前の結果を捨てる（決定 2・9）。
void verify_switch_requests_now()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"first"});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"second"});
    applied(controller, app::TogglePreview{});
    auto &markdown = editing.markdown();
    markdown.serve(parsed(1, "second"));
    applied(controller, app::WorkCompleted{});
    applied(controller, app::InsertText{"!"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    const EditorFrame switched = controller.apply_frame(app::SwitchTab{0});
    const auto *preview = preview_of(switched);
    expect(markdown.requests().size() == 3 && markdown.requests().back().ticket == 3 &&
               *markdown.requests().back().text == "first",
           "switching the tab requests the new document at once with the next ticket");
    expect(preview != nullptr && preview->document == nullptr,
           "switching the tab drops the result of the previous document");
    applied(controller, app::NewTab{});
    expect(markdown.requests().size() == 4 && markdown.requests().back().text->empty(),
           "a new tab requests its empty body at once");
}

// 5. プレビューの枠は操作中にならない。閉じる・`:q`・分けるの道（決定 4）。
void verify_pane_intents()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"body"});
    applied(controller, app::TogglePreview{});
    const EditorFrame refused = controller.apply_frame(app::SwitchPane{});
    expect(preview_of(refused) != nullptr && refused.active_side == core::PaneSide::first &&
               message_of(refused) == "プレビューの枠には入れません",
           "SwitchPane keeps the preview and says one line");
    const EditorFrame closed = controller.apply_frame(app::ClosePane{});
    expect(closed.layout == SplitLayout::none && !closed.other.has_value() &&
               closed.tabs.size() == 1 && closed.lines.front().text == "body",
           "ClosePane closes the preview and keeps the body");
    applied(controller, app::TogglePreview{});
    const EditorFrame split = controller.apply_frame(app::SplitPane{SplitLayout::horizontal});
    expect(split.layout == SplitLayout::horizontal && split.other.has_value() &&
               std::holds_alternative<app::PaneFrame>(split.other.value()),
           "SplitPane drops the preview and puts a pane of the body");
    const EditorFrame replaced = controller.apply_frame(app::TogglePreview{});
    expect(replaced.layout == SplitLayout::horizontal && preview_of(replaced) != nullptr,
           "TogglePreview replaces the other pane with the preview");
    const EditorFrame toggled = controller.apply_frame(app::TogglePreview{});
    expect(toggled.layout == SplitLayout::none && !toggled.other.has_value(),
           "TogglePreview again closes the preview");

    Editing vim;
    open_vim_document(vim, "one\ntwo");
    auto &typed = vim.controller();
    vim_replay(typed, "x");
    applied(typed, app::TogglePreview{});
    const EditorFrame quit = run_ex(typed, "q");
    expect(quit.layout == SplitLayout::none && !quit.closing && quit.tabs.size() == 1 &&
               message_of(quit).empty(),
           ":q with the preview closes only the preview");
    applied(typed, app::TogglePreview{});
    static_cast<void>(typed.press_vim_key(core::VimKey{core::VimSpecialKey::control_w}));
    vim_replay(typed, "c");
    expect(!typed.frame().other.has_value() && typed.frame().tabs.size() == 1,
           "Ctrl-w c with the preview closes only the preview");
}

// 6. 16 MiB を超える本文は頼まず 1 行（決定 3）。
void verify_size_limit()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{std::string(core::preview_byte_limit + 1, 'x')});
    const EditorFrame opened = controller.apply_frame(app::TogglePreview{});
    const auto *preview = preview_of(opened);
    expect(editing.markdown().requests().empty() && preview != nullptr &&
               notice_of(*preview) == "プレビューは 16 MiB までです",
           "a body over 16 MiB is not requested and the preview says one line");
}

// 7. 錨は本文の行で動き、本文が送られると追従に戻る（決定 7）。
void verify_anchor()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::InsertText{"l1\nl2\nl3\nl4\nl5\nl6\nl7\nl8"});
    applied(controller, app::ScrollLines{-100});
    const EditorFrame opened = controller.apply_frame(app::TogglePreview{});
    expect(preview_of(opened)->anchor == core::Offset{0},
           "a following preview anchors at the first visible line of the body");
    const EditorFrame moved = controller.apply_frame(app::ScrollPane{2});
    expect(preview_of(moved)->anchor == core::Offset{6} && moved.first_visible.value == 1,
           "ScrollPane moves the anchor by body lines and leaves the body");
    const EditorFrame stayed = controller.apply_frame(app::ScrollPane{-10});
    expect(preview_of(stayed)->anchor == core::Offset{0}, "the anchor stops at the first line");
    applied(controller, app::ScrollPane{100});
    expect(preview_of(controller.frame())->anchor == core::Offset{21},
           "the anchor stops at the last line");
    const EditorFrame scrolled = controller.apply_frame(app::ScrollLines{1});
    expect(preview_of(scrolled)->anchor == core::Offset{3} && scrolled.first_visible.value == 2,
           "scrolling the body makes the preview follow again");
}

// 8. tick は使う人の操作ではない。歩き・`.`・undo を動かさない（決定 8）。
void verify_tick_keeps_state()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"b"});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"c"});
    applied(controller, app::TogglePreview{});
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, app::PreviewTick{});
    applied(controller, app::PreviewTick{});
    expect(controller.tab_walking(), "a preview tick does not settle the walk");
    const EditorFrame third = controller.apply_frame(app::WalkRecentTab{core::TabStep::next});
    expect(!third.lines.empty() && third.lines.front().text == "a",
           "the next walk after the ticks reaches the third tab in the order");
    applied(controller, app::SettleRecentTab{});

    Editing plain;
    open_vim_document(plain, "abc\nabc");
    applied(plain.controller(), app::TogglePreview{});
    vim_replay(plain.controller(), "x.uu");
    Editing ticked;
    open_vim_document(ticked, "abc\nabc");
    applied(ticked.controller(), app::TogglePreview{});
    vim_replay(ticked.controller(), "x");
    applied(ticked.controller(), app::PreviewTick{});
    applied(ticked.controller(), app::PreviewTick{});
    vim_replay(ticked.controller(), ".");
    applied(ticked.controller(), app::PreviewTick{});
    vim_replay(ticked.controller(), "u");
    applied(ticked.controller(), app::PreviewTick{});
    vim_replay(ticked.controller(), "u");
    expect(whole_vim_body(plain.controller()) == whole_vim_body(ticked.controller()) &&
               whole_vim_body(ticked.controller()) == "abc\nabc" &&
               ticked.markdown().requests().size() == 2,
           "ticks between x, . and u never move the dot record nor the undo units");
}

// 9. プレビューが開いていない間は Markdown の口を呼ばない（決定 10）。
void verify_closed_is_quiet()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::PreviewTick{});
    applied(controller, app::WorkCompleted{});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, app::PreviewTick{});
    applied(controller, app::WorkCompleted{});
    applied(controller, app::ClosePane{});
    expect(editing.markdown().requests().empty() && editing.markdown().collects() == 0,
           "ticks and signals without a preview never touch the markdown port");
    applied(controller, app::TogglePreview{});
    applied(controller, app::TogglePreview{});
    applied(controller, app::WorkCompleted{});
    applied(controller, app::PreviewTick{});
    expect(editing.markdown().requests().size() == 1 && editing.markdown().collects() == 1,
           "after closing the preview the signal and the tick stay quiet");
}

// 10. 閉じて開き直しても券は戻らない。閉じる前の券の結果は開き直した後に捨てる（決定 6・R1）。
void verify_reopen_drops_old_ticket()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"body"});
    applied(controller, app::TogglePreview{});
    applied(controller, app::TogglePreview{});
    applied(controller, app::TogglePreview{});
    auto &markdown = editing.markdown();
    expect(markdown.requests().size() == 2 && markdown.requests().back().ticket == 2,
           "opening again requests with the next ticket, not the first one");
    markdown.serve(parsed(1, "before closing"));
    const EditorFrame stale = controller.apply_frame(app::WorkCompleted{});
    expect(preview_of(stale) != nullptr && preview_of(stale)->document == nullptr,
           "the result of the ticket before closing is dropped after opening again");
    markdown.serve(parsed(2, "after opening"));
    const EditorFrame fresh = controller.apply_frame(app::WorkCompleted{});
    expect(shown_text(*preview_of(fresh)) == "after opening",
           "only the result of the ticket after opening again is shown");
}

// 11. 閉じる道は全部プレビューだけを閉じ、本文と窓は残す（決定 4・R2）。`:wq` と `:x` は
// 保存が走ってもプレビューだけを閉じる。
void verify_close_paths()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    applied(controller, app::TogglePreview{});
    window_command(controller, "q");
    const EditorFrame quit = controller.frame();
    expect(quit.layout == SplitLayout::none && !quit.closing && quit.tabs.size() == 1,
           "Ctrl-w q with the preview closes only the preview");
    applied(controller, app::TogglePreview{});
    window_command(controller, "o");
    expect(!controller.frame().other.has_value() && controller.frame().tabs.size() == 1,
           "Ctrl-w o with the preview closes only the preview");
    applied(controller, app::TogglePreview{});
    const EditorFrame only = run_ex(controller, "only");
    expect(only.layout == SplitLayout::none && !only.other.has_value() && only.tabs.size() == 1,
           ":only with the preview closes only the preview");
    vim_replay(controller, "x");
    applied(controller, app::TogglePreview{});
    const EditorFrame forced = run_ex(controller, "q!");
    expect(forced.layout == SplitLayout::none && !forced.closing && forced.tabs.size() == 1 &&
               forced.document.save_state == core::SaveState::modified,
           ":q! with the preview closes only the preview and keeps the unsaved body");
    applied(controller, app::TogglePreview{});
    const EditorFrame written = run_ex(controller, "wq");
    expect(written.layout == SplitLayout::none && !written.closing && written.tabs.size() == 1 &&
               editing.files().written() == "ne\ntwo" &&
               written.document.save_state == core::SaveState::saved,
           ":wq with the preview writes and then closes only the preview");
    vim_replay(controller, "x");
    applied(controller, app::TogglePreview{});
    const EditorFrame exited = run_ex(controller, "x");
    expect(exited.layout == SplitLayout::none && !exited.closing && exited.tabs.size() == 1 &&
               editing.files().written() == "e\ntwo" &&
               exited.document.save_state == core::SaveState::saved,
           ":x with the preview writes and then closes only the preview");
}

// 12. 開いたまま文書を開く・閉じたタブの隣へ移ると、tick を待たずに頼む（決定 9・I9）。
void verify_open_and_close_request_now()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\work\\a.md", Bytes{"alpha"});
    editing.files().hold_at("C:\\work\\b.md", Bytes{"beta"});
    applied(controller, app::VisibleLines{10});
    applied(controller, app::OpenDocument{core::FilePath::parse("C:\\work\\a.md").value()});
    applied(controller, app::TogglePreview{});
    auto &markdown = editing.markdown();
    const EditorFrame opened =
        controller.apply_frame(app::OpenDocument{core::FilePath::parse("C:\\work\\b.md").value()});
    expect(opened.tabs.size() == 2 && markdown.requests().size() == 2 &&
               markdown.requests().back().ticket == 2 && *markdown.requests().back().text == "beta",
           "opening a file with the preview requests the new document at once");
    const EditorFrame closed = controller.apply_frame(app::CloseTab{1});
    expect(closed.tabs.size() == 1 && preview_of(closed) != nullptr &&
               markdown.requests().size() == 3 && markdown.requests().back().ticket == 3 &&
               *markdown.requests().back().text == "alpha",
           "closing the tab requests its neighbour at once");
}

// 13. プレビューが開いているときの合図 WorkCompleted も、歩き・`.`・undo を動かさない
// （--background-work の same_with_work と同じ形・I9）。
void verify_work_keeps_state()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"b"});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"c"});
    applied(controller, app::TogglePreview{});
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    const auto ticket = editing.markdown().requests().back().ticket;
    editing.markdown().serve(parsed(ticket, "b"));
    applied(controller, app::WorkCompleted{});
    expect(controller.tab_walking(), "a work signal with the preview does not settle the walk");
    const EditorFrame third = controller.apply_frame(app::WalkRecentTab{core::TabStep::next});
    expect(!third.lines.empty() && third.lines.front().text == "a",
           "the next walk after the signal reaches the third tab in the order");
    applied(controller, app::SettleRecentTab{});

    Editing plain;
    open_vim_document(plain, "abc\nabc");
    applied(plain.controller(), app::TogglePreview{});
    vim_replay(plain.controller(), "x.uu");
    Editing worked;
    open_vim_document(worked, "abc\nabc");
    applied(worked.controller(), app::TogglePreview{});
    vim_replay(worked.controller(), "x");
    worked.markdown().serve(parsed(1, "abc"));
    applied(worked.controller(), app::WorkCompleted{});
    vim_replay(worked.controller(), ".");
    applied(worked.controller(), app::WorkCompleted{});
    vim_replay(worked.controller(), "u");
    applied(worked.controller(), app::WorkCompleted{});
    vim_replay(worked.controller(), "u");
    expect(whole_vim_body(plain.controller()) == whole_vim_body(worked.controller()) &&
               whole_vim_body(worked.controller()) == "abc\nabc" &&
               shown_text(*preview_of(worked.controller().frame())) == "abc",
           "work signals between x, . and u never move the dot record nor the undo units");
}
} // namespace

void verify_preview_contracts()
{
    verify_open_requests();
    verify_ticket_results();
    verify_tick_thinning();
    verify_quiet_ticks_do_not_repaint();
    verify_tick_keeps_wanted_column();
    verify_tick_keeps_vim_wanted_column();
    verify_switch_requests_now();
    verify_pane_intents();
    verify_size_limit();
    verify_anchor();
    verify_tick_keeps_state();
    verify_closed_is_quiet();
    verify_reopen_drops_old_ticket();
    verify_close_paths();
    verify_open_and_close_request_now();
    verify_work_keeps_state();
}

void verify_preview_scope()
{
    verify_preview_contracts();
}
} // namespace nenenib::tests
