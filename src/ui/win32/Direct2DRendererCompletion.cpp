// 補完の面の描画（ADR 0169 の決定 4・ADR 0190 の決定 5）。Direct2DRenderer.cpp の長さの上限
// （CNF-014）のため翻訳単位を分けた。
#include "Direct2DRenderer.hpp"

#include "CompletionFace.hpp"
#include "CompletionFaceRequest.hpp"
#include "CompletionPainter.hpp"
#include "CompletionView.hpp"
#include "ContextMenuText.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"

#include <utility>

namespace nenenib::ui::win32
{
// 面はキャレットの下（入らなければ上）。キャレットの矩形はこの frame の本文を描いたときの値で、
// 面の左端は面を開いたとき（通し番号が変わったとき）のキャレットの左に留める（打っている間に面を
// 横へ動かさない）。bounds は帯とステータスバーを除いた本文の面。
void Direct2DRenderer::draw_completion(const application::EditorFrame &frame,
                                       const core::LayoutRect &bounds)
{
    if (!frame.completion.has_value())
    {
        // 閉じたら文字組みと左端の覚えを捨てる（右クリックのメニューと同じ）。
        completion_layouts_.clear();
        completion_anchor_.reset();
        return;
    }
    const auto &view = frame.completion.value();
    if (!completion_anchor_.has_value() || completion_anchor_.value().first != view.session)
    {
        completion_anchor_ =
            std::pair{view.session, static_cast<std::int32_t>(caret_rectangle_.left)};
    }
    const core::LayoutRect caret{caret_rectangle_.left, caret_rectangle_.top,
                                 caret_rectangle_.right, caret_rectangle_.bottom};
    const auto layout = core::completion_face_layout(
        core::CompletionFaceRequest{caret, completion_anchor_.value().second, bounds,
                                    view.rows.size(), view.selected, view.detailed, dpi_});
    CompletionPainter painter(
        *context_.Get(), *brush_.Get(),
        ContextMenuText{dwrite_.Get(), command_format_.Get(), key_format_.Get(), dpi_},
        completion_layouts_);
    painter.paint(view, layout, frame.palette);
}
} // namespace nenenib::ui::win32
