// プレビューの縦の送りの application の側（ADR 0155 の決定 7 の 2026-11-20 の追補・#1894）。
// 送りは量（pending）だけを積み、ui が畳んだ答え PreviewScrollSettled で錨と nudge を置いて畳んだ
// 分を引く。囲みのコードは枠の上端の行の行頭へ錨を写す。本文が送られると追従に戻って 0 に戻す。
// 答えは合図で、Ctrl+Tab の歩き・Vim の待ち・録画・`.`・undo を動かさない。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LineNumber.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownStop.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPlacement.hpp"
#include "PreviewScrollSettled.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "SelectEditMode.hpp"
#include "SetPreviewPlacement.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TogglePreview.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WalkRecentTab.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;

void serve_parsed(Editing &editing)
{
    const auto &request = editing.markdown().requests().back();
    auto parsed = core::parse_markdown(*request.text, core::MarkdownStop::never());
    editing.markdown().serve(app::MarkdownOutcome{request.ticket, std::move(parsed)});
    applied(editing.controller(), app::WorkCompleted{});
}

void open_previewed(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    applied(editing.controller(), app::TogglePreview{});
    serve_parsed(editing);
}

[[nodiscard]] const app::PreviewFrame &preview_of(const EditorFrame &frame)
{
    if (frame.full_preview.has_value())
    {
        return frame.full_preview.value();
    }
    static const app::PreviewFrame none{nullptr, core::Offset{0}, {}, {}};
    const auto *other =
        frame.other.has_value() ? std::get_if<app::PreviewFrame>(&frame.other.value()) : nullptr;
    return other == nullptr ? none : *other;
}

[[nodiscard]] std::size_t line_of(Editing &editing, core::Offset at)
{
    return editing.controller().text().position_of(at).line.value;
}

// 見出し・空行・400 行の囲み・空行・段落（囲みは 128 行ごとの切れ・#1863）。
[[nodiscard]] std::string long_fence()
{
    std::string text = "# h\n\n```\n";
    for (int line = 1; line <= 400; ++line)
    {
        text += "code " + std::to_string(line) + "\n";
    }
    return text + "```\n\nafter\n";
}

// 1. 送りは量だけを積み、錨と本文と追従の印を決める。
void verify_pending()
{
    Editing editing;
    open_previewed(editing, long_fence());
    auto &controller = editing.controller();
    const EditorFrame opened = controller.frame();
    expect(preview_of(opened).pending == 0 && preview_of(opened).nudge == 0.0F,
           "a following preview has nothing pending");
    applied(controller, app::ScrollPane{3});
    applied(controller, app::ScrollPane{3});
    const EditorFrame sent = controller.apply_frame(app::ScrollPane{-2});
    expect(preview_of(sent).pending == 4 && preview_of(sent).anchor == core::Offset{0} &&
               sent.first_visible.value == 1,
           "the wheel adds signed lines from the followed line and leaves the body");
}

// 2. 答えは錨と nudge を置き、畳んだ分だけを引く（間に来た送りを残す）。
void verify_settled()
{
    Editing editing;
    open_previewed(editing, long_fence());
    auto &controller = editing.controller();
    applied(controller, app::ScrollPane{3});
    applied(controller, app::ScrollPane{3});
    const auto &blocks = preview_of(controller.frame()).document->blocks;
    const auto after = blocks.back().source_begin;
    const EditorFrame settled =
        controller.apply_frame(app::PreviewScrollSettled{after, 0, 7.25F, 3});
    expect(preview_of(settled).anchor == after && preview_of(settled).nudge == 7.25F &&
               preview_of(settled).pending == 3,
           "the answer places the anchor and the nudge and keeps the amount sent after it");
    const EditorFrame rest = controller.apply_frame(app::PreviewScrollSettled{after, 0, 9.0F, 3});
    expect(preview_of(rest).pending == 0 && preview_of(rest).nudge == 9.0F,
           "the next answer takes the rest");
}

// 3. 囲みのコードの答えは、枠の上端の行の行頭へ錨を写す（同じ要素の中だけ）。
void verify_code_line()
{
    Editing editing;
    open_previewed(editing, long_fence());
    auto &controller = editing.controller();
    applied(controller, app::ScrollPane{3});
    const auto &blocks = preview_of(controller.frame()).document->blocks;
    const auto second = blocks.at(2).source_begin;
    const auto first_line = line_of(editing, second);
    const EditorFrame inside =
        controller.apply_frame(app::PreviewScrollSettled{second, 5, 100.0F, 3});
    expect(line_of(editing, preview_of(inside).anchor) == first_line + 5 &&
               preview_of(inside).nudge == 100.0F,
           "a code piece anchors at the line under the top of the frame");
    applied(controller, app::ScrollPane{3});
    const EditorFrame beyond =
        controller.apply_frame(app::PreviewScrollSettled{second, 500, 100.0F, 3});
    expect(preview_of(beyond).anchor == second,
           "a line past the piece keeps the anchor at the piece");
}

// 4. 本文が送られたら追従に戻り、nudge と送りを捨てる。追従中の答えは古いので捨てる。
void verify_follow()
{
    Editing editing;
    open_previewed(editing, long_fence());
    auto &controller = editing.controller();
    applied(controller, app::ScrollPane{3});
    const auto after = preview_of(controller.frame()).document->blocks.back().source_begin;
    applied(controller, app::PreviewScrollSettled{after, 0, 5.0F, 3});
    applied(controller, app::ScrollPane{3});
    const EditorFrame followed = controller.apply_frame(app::ScrollLines{2});
    expect(preview_of(followed).nudge == 0.0F && preview_of(followed).pending == 0 &&
               preview_of(followed).anchor == controller.text().line_start(followed.first_visible),
           "scrolling the body follows again and drops the nudge and what was pending");
    const EditorFrame stale = controller.apply_frame(app::PreviewScrollSettled{after, 0, 5.0F, 0});
    expect(preview_of(stale).anchor == controller.text().line_start(stale.first_visible) &&
               preview_of(stale).nudge == 0.0F,
           "an answer while following is dropped");
}

// 5. 全面へ移しても送りと nudge は同じ PreviewView（改訂 15）。`G` は置き直して 0。
void verify_full()
{
    Editing editing;
    open_previewed(editing, long_fence());
    auto &controller = editing.controller();
    applied(controller, app::ScrollPane{3});
    const auto after = preview_of(controller.frame()).document->blocks.back().source_begin;
    applied(controller, app::PreviewScrollSettled{after, 0, 5.0F, 3});
    applied(controller, app::ScrollPane{3});
    const EditorFrame full =
        controller.apply_frame(app::SetPreviewPlacement{app::PreviewPlacement::full});
    expect(full.full_preview.has_value() && preview_of(full).nudge == 5.0F &&
               preview_of(full).pending == 3 && preview_of(full).anchor == after,
           "the full preview keeps the anchor, the nudge and what is pending");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_replay(controller, "G");
    const EditorFrame last = controller.frame();
    expect(preview_of(last).nudge == 0.0F && preview_of(last).pending == 0,
           "G places the anchor again with nothing pending");
}

// 6. 答えは合図: Ctrl+Tab の歩きを確定せず、Vim の待ち・録画・`.`・undo を動かさない。
void verify_signal()
{
    Editing walking;
    open_previewed(walking, long_fence());
    auto &controller = walking.controller();
    applied(controller, app::NewTab{});
    applied(controller, app::WalkRecentTab{core::TabStep::next});
    applied(controller, app::PreviewScrollSettled{core::Offset{0}, 0, 0.0F, 0});
    expect(controller.tab_walking(), "the answer keeps the Ctrl+Tab walk");
    const auto same_with_answer = [](std::string_view before, std::string_view after)
    {
        Editing plain;
        open_vim_document(plain, "one two three");
        vim_replay(plain.controller(), std::string(before) + std::string(after));
        Editing answered;
        open_vim_document(answered, "one two three");
        vim_replay(answered.controller(), before);
        applied(answered.controller(), app::PreviewScrollSettled{core::Offset{0}, 0, 0.0F, 0});
        vim_replay(answered.controller(), after);
        return whole_vim_body(plain.controller()) == whole_vim_body(answered.controller());
    };
    expect(same_with_answer("d", "w"), "an operator waiting for a motion goes on after the answer");
    expect(same_with_answer("qa", "xq@a"), "the answer never enters a macro being recorded");
    expect(same_with_answer("x", "."), "the answer never replaces the last change for dot");
    expect(same_with_answer("ia", "b<Esc>u"), "the answer never splits an insert's undo unit");
}
} // namespace

void verify_preview_scroll_scope()
{
    verify_pending();
    verify_settled();
    verify_code_line();
    verify_follow();
    verify_full();
    verify_signal();
}
} // namespace nenenib::tests
