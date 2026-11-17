// イージーモーションの印の描画（ADR 0163 の追補 #1800・案 D'）。札・印の書式・全部の行の後の印の
// 1 パスとその後のキャレット。Direct2DRenderer.cpp の長さの上限（CNF-014）のため翻訳単位を分けた。
#include "Direct2DRenderer.hpp"
#include "Direct2DRendererDetail.hpp"
#include "TextLayoutPoint.hpp"

#include "BlockCaretOutline.hpp"
#include "BodyPane.hpp"
#include "CaretAffinity.hpp"
#include "Column.hpp"
#include "EditorFrame.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "LineUtf16Evaluation.hpp"
#include "LineView.hpp"
#include "MotionLabel.hpp"
#include "MotionLabelBox.hpp"
#include "MotionLabelPaint.hpp"
#include "PaneRole.hpp"
#include "RenderFailure.hpp"
#include "SelectionSpan.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
constexpr std::int32_t label_room = 4096;

// 行の印を桁の順（左から右）に並べる。印の列は近い順なので、重なる札が右を上にするよう並べ直す。
[[nodiscard]] std::vector<const core::MotionLabel *>
labels_left_to_right(const application::LineView &line)
{
    std::vector<const core::MotionLabel *> ordered;
    ordered.reserve(line.labels.size());
    for (const auto &label : line.labels)
    {
        ordered.push_back(&label);
    }
    std::ranges::sort(ordered, [](const core::MotionLabel *left, const core::MotionLabel *right)
                      { return left->column.value < right->column.value; });
    return ordered;
}
} // namespace

using detail::body_caret_hidden;
using detail::line_area;
using detail::to_color;
using detail::to_rect;

void Direct2DRenderer::draw_line_labels(const application::EditorFrame &frame,
                                        IDWriteTextLayout *text, const core::LayoutRect &area,
                                        const application::LineView &line)
{
    LineUtf16Evaluation positions(line.display.text);
    for (const auto *placed : labels_left_to_right(line))
    {
        const auto &label = *placed;
        const core::SelectionSpan target{core::SelectionPresence::present, label.column,
                                         core::Column{label.column.value + 1}};
        const DWRITE_TEXT_RANGE range = range_of(line, target, positions);
        const auto runs = runs_of(text, area, range);
        if (!runs.empty())
        {
            draw_label(frame, runs.front(), label);
            continue;
        }
        // 行の終わり（空の行を含む）の跳び先は字が無いので、その位置の点と行の高さから当たりを
        // 作る（札は字のある跳び先と同じく行の中の上寄せ）。
        const auto point =
            text_layout_point(text, range.startPosition, core::CaretAffinity::leading);
        if (!point)
        {
            text_failure_ = RenderFailure::directwrite;
            continue;
        }
        DWRITE_HIT_TEST_METRICS end{};
        end.left = static_cast<float>(area.left) + point.value().x;
        end.top = static_cast<float>(area.top) + point.value().y;
        end.height = static_cast<float>(body_line_height_);
        draw_label(frame, end, label);
    }
}

// 札は motion_label_badge（跳び先の行の中の上寄せ・空の行と行末も同じ）。字は印の書式の layout を
// 測ってから札の大きさに広げて中央に置き、札に切り抜いて描く（paint_motion_label・tests/ui と
// 同じ 1 本）。
void Direct2DRenderer::draw_label(const application::EditorFrame &frame,
                                  const DWRITE_HIT_TEST_METRICS &target,
                                  const core::MotionLabel &label)
{
    const auto written = text_layout(label.label, label_format_.Get(),
                                     core::LayoutRect{0, 0, label_room, label_room});
    DWRITE_TEXT_METRICS measured{};
    if (!written || FAILED(written->GetMetrics(&measured)))
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    const auto size = D2D1::SizeF(measured.widthIncludingTrailingWhitespace, measured.height);
    const D2D1_ROUNDED_RECT badge = motion_label_badge(target, label, size, dpi_);
    const auto &shape = badge.rect;
    if (FAILED(written->SetMaxWidth(shape.right - shape.left)) ||
        FAILED(written->SetMaxHeight(shape.bottom - shape.top)))
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    paint_motion_label(*context_.Get(), brush_.Get(),
                       MotionLabelPaint{badge, written.Get(), to_color(frame.palette.text),
                                        to_color(frame.palette.background)});
}

bool Direct2DRenderer::ensure_label_format()
{
    if (label_format_)
    {
        return true;
    }
    const std::wstring requested = core::to_utf16(formatted_family_.text()).value_or(L"");
    const wchar_t *face = family(requested.c_str(), L"Consolas");
    TextFormat made;
    if (FAILED(make_format(face, motion_label_text_dips(core::font_size_dips(formatted_size_)),
                           DWRITE_FONT_WEIGHT_BOLD, made)) ||
        FAILED(made->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER)) ||
        FAILED(made->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)) ||
        FAILED(made->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)))
    {
        return false;
    }
    label_format_ = std::move(made);
    return true;
}

// 印の 1 パス（ADR 0163 の追補 #1800・案 D'）。全部の行（本文・面・枠）を描いた後に、上の行から
// 下・左から右の順に描き、後の札が上に来る（札は行の中に収まるが、半角の跳び先の 2 文字の札は
// 次の升へ出るので、隣の升の面と枠より上に描く順を 1 か所に保つ）。札は本文の欄（行番号の欄を
// 除く）で切り、その後にキャレット（印が出ている間は枠だけ）を描く。印の無い frame は入口で抜ける。
void Direct2DRenderer::draw_label_pass(const application::EditorFrame &frame, const BodyPane &pane)
{
    if (pane.role != PaneRole::active || !motion_labels_shown(pane.lines))
    {
        return;
    }
    if (!ensure_label_format())
    {
        text_failure_ = RenderFailure::directwrite;
        return;
    }
    const auto &body = pane.body;
    context_->PushAxisAlignedClip(to_rect(body.content), D2D1_ANTIALIAS_MODE_ALIASED);
    for (const auto &line : pane.lines)
    {
        if (line.labels.empty() || line.composed.has_value())
        {
            continue;
        }
        const auto text = layout_of(line.display.text, body, frame.settings, line.window);
        if (text)
        {
            draw_line_labels(frame, text.Get(), line_area(body, line), line);
        }
    }
    draw_pass_caret(frame, pane);
    context_->PopAxisAlignedClip();
}

// 印の 1 パスの最後のキャレット（札の後・入力行が開いていれば描かない・IME の差し込み行は
// 差し込みの道が描く）。
void Direct2DRenderer::draw_pass_caret(const application::EditorFrame &frame, const BodyPane &pane)
{
    if (body_caret_hidden(frame))
    {
        return;
    }
    for (const auto &line : pane.lines)
    {
        if (line.number != pane.caret.position.line || line.composed.has_value())
        {
            continue;
        }
        const auto text = layout_of(line.display.text, pane.body, frame.settings, line.window);
        if (text)
        {
            draw_caret(frame, text.Get(), line_area(pane.body, line), line);
        }
    }
}
} // namespace nenenib::ui::win32
