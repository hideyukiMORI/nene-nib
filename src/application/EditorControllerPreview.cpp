#include "EditorController.hpp"

#include "DisplayText.hpp"
#include "EditBoundary.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownRequest.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PreviewView.hpp"
#include "TogglePreviewTask.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::application
{
namespace
{
// プレビューの 1 行（ADR 0155 の決定 3・6）。
constexpr std::string_view preview_too_large_message = "プレビューは 16 MiB までです";
constexpr std::string_view preview_rejected_message = "Markdown を解析できませんでした";
constexpr std::string_view preview_memory_message = "メモリが足りずプレビューできませんでした";
// チェックボックスの印が本文で `[ ]` `[x]` `[X]` の形でないとき（ADR 0155 の追補 24）。
constexpr std::string_view preview_task_stale_message = "プレビューが古いので切り替えられません";

// 見せている結果の block の印の位置（タスクでない・範囲の外なら nullopt）。
[[nodiscard]] std::optional<std::size_t> task_mark_of(const PreviewView &preview, std::size_t block)
{
    if (preview.document == nullptr || block >= preview.document->blocks.size())
    {
        return std::nullopt;
    }
    const auto &task = preview.document->blocks.at(block).task;
    if (!task.has_value())
    {
        return std::nullopt;
    }
    return task.value().mark_offset;
}

// 印の前後を含む 3 バイトが `[ ]` `[x]` `[X]` か。
[[nodiscard]] bool task_mark_shape(std::string_view around) noexcept
{
    return around.size() == 3U && around.front() == '[' && around.back() == ']' &&
           (around.at(1) == ' ' || around.at(1) == 'x' || around.at(1) == 'X');
}
} // namespace

// ---------------------------------------------------------------- プレビュー（ADR 0155）

// 開く・閉じるの 1 本（決定 3）。開く前に、前に開いていたときの残りを捨てる（たまりが空でないと
// 次の合図が来ない・ADR 0062 の決定 15 と同じ）。
void EditorController::accept(const TogglePreview &)
{
    if (state_.preview() != nullptr)
    {
        state_ = state_.without_split();
        return;
    }
    static_cast<void>(ports_.markdown.collect());
    settle_follow();
    state_ = state_.with_preview(PreviewView{});
    request_preview(true);
}

// チェックボックスの切り替え（ADR 0155 の追補 24）。見せている結果の印の位置を今の本文で確かめ、
// `[ ]` なら `x`、`[x]` `[X]` なら ` ` に置き換える Edit 1 つ（undo 1 単位・restore はそのときの
// キャレット）。プレビューの枠を押しても本文のキャレットは動かないので（ADR 0155）、選択と追従の
// 頼みは編集の前のまま残す（1 バイトを 1 バイトに置き換えるので位置はずれない）。描き直しは改訂
// 番号が進んだ本文を PreviewTick が頼む道（決定 8）で、新しい要求の道は足さない。
void EditorController::accept(const TogglePreviewTask &intent)
{
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    const auto mark = task_mark_of(*preview, intent.block);
    const std::size_t size = state_.text().size_bytes();
    if (!mark.has_value() || mark.value() == 0U || mark.value() + 2U > size ||
        !task_mark_shape(state_.text().text_range(core::Offset{mark.value() - 1U},
                                                  core::Offset{mark.value() + 2U})))
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse(preview_task_stale_message).value());
        return;
    }
    const core::Offset at{mark.value()};
    const bool checked = state_.text().text_range(at, core::Offset{at.value + 1U}) != " ";
    const auto selection = state_.selection();
    const auto follow = pending_follow_;
    replace(core::OffsetRange{at, core::Offset{at.value + 1U}}, checked ? " " : "x",
            core::EditBoundary::separate);
    state_ = state_.with_selection(selection);
    pending_follow_ = follow;
}

// 打つのを止めてから 1 tick 以上たった本文だけを頼む（決定 8）。比べるのは履歴の位置ではなく
// 改訂番号。頼まなかった合図は見えるものを変えない（見た改訂番号は描かない）ので描き直さない
// （ADR 0155 の追補・#813）。結果を採るのは WorkCompleted の道。
void EditorController::accept(const PreviewTick &)
{
    repaint_ = false;
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    const std::size_t revision = state_.history().revision();
    if (revision == preview->seen_revision && revision != preview->requested_revision)
    {
        repaint_ = true;
        request_preview(false);
        return;
    }
    if (revision != preview->seen_revision)
    {
        PreviewView view = *preview;
        view.seen_revision = revision;
        state_ = state_.with_preview(std::move(view));
    }
}

void EditorController::request_preview(bool fresh)
{
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    const std::size_t revision = state_.history().revision();
    view.requested_revision = revision;
    view.seen_revision = revision;
    if (fresh)
    {
        view.document.reset();
        view.notice.reset();
        view.follows = true;
    }
    // 券は頼まなくても進め、前の要求の結果を捨てる（決定 6）。
    view.ticket = ++preview_tickets_;
    if (state_.text().size_bytes() > core::preview_byte_limit)
    {
        view.notice = core::DisplayText::parse(preview_too_large_message).value();
        state_ = state_.with_preview(std::move(view));
        return;
    }
    auto text = std::make_shared<const std::string>(state_.text().text());
    ports_.markdown.parse(MarkdownRequest{view.ticket, std::move(text)});
    state_ = state_.with_preview(std::move(view));
}

void EditorController::collect_preview()
{
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    bool changed = false;
    for (MarkdownOutcome &outcome : ports_.markdown.collect())
    {
        if (outcome.ticket != view.ticket)
        {
            continue;
        }
        if (outcome.result)
        {
            view.document =
                std::make_shared<const core::MarkdownDocument>(std::move(outcome.result).value());
            view.notice.reset();
            changed = true;
            continue;
        }
        // 差し替わるまで前の結果を出したまま、1 行だけ添える（決定 6）。
        switch (outcome.result.error())
        {
        case core::MarkdownFailure::stopped:
            continue;
        case core::MarkdownFailure::too_large:
            view.notice = core::DisplayText::parse(preview_too_large_message).value();
            break;
        case core::MarkdownFailure::parser_rejected:
            view.notice = core::DisplayText::parse(preview_rejected_message).value();
            break;
        case core::MarkdownFailure::insufficient_memory:
            view.notice = core::DisplayText::parse(preview_memory_message).value();
            break;
        }
        changed = true;
    }
    if (changed)
    {
        state_ = state_.with_preview(std::move(view));
    }
}

ScrollState EditorController::framed_scroll()
{
    const ScrollState scroll = settled_scroll();
    follow_preview_on_scroll(scroll.first_visible);
    return scroll;
}

void EditorController::follow_preview_on_scroll(core::LineNumber first_visible)
{
    const auto previous = std::exchange(preview_body_top_, first_visible);
    const PreviewView *preview = state_.preview();
    if (preview == nullptr || preview->follows || !previous.has_value() ||
        previous.value() == first_visible)
    {
        return;
    }
    PreviewView view = *preview;
    view.follows = true;
    state_ = state_.with_preview(std::move(view));
}
} // namespace nenenib::application
