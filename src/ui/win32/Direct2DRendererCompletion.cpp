// 補完の面の描画と当たり（ADR 0169 の決定 4・ADR 0190 の決定 5）。Direct2DRenderer.cpp の長さの
// 上限（CNF-014）のため翻訳単位を分けた。
#include "Direct2DRenderer.hpp"

#include "CompletionFace.hpp"
#include "CompletionFaceRequest.hpp"
#include "CompletionPainter.hpp"
#include "CompletionView.hpp"
#include "ContextMenuText.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// 点の下の候補（今の接頭辞に当たる候補の中の位置・表示値の first + 行）。面を描いていなければ無し。
std::optional<std::size_t> Direct2DRenderer::completion_row_at(std::int32_t x, std::int32_t y) const
{
    if (!completion_layout_.has_value())
    {
        return std::nullopt;
    }
    const auto row = core::completion_face_hit(completion_layout_.value().first, x, y);
    return row.has_value() ? std::optional{completion_layout_.value().second + row.value()}
                           : std::nullopt;
}

bool Direct2DRenderer::completion_covers(std::int32_t x, std::int32_t y) const
{
    return completion_layout_.has_value() &&
           core::contains(completion_layout_.value().first.panel, x, y);
}

// 面はキャレットの下（入らなければ上）。キャレットの矩形はこの frame の本文を描いたときの値で、
// 面の左端は面を開いたとき（通し番号が変わったとき）のキャレットの左に留める（打っている間に面を
// 横へ動かさない）。bounds は帯とステータスバーを除いた本文の面。描いた配置は当たりのために覚える
// （描画と当たりが同じ core::completion_face_layout）。
void Direct2DRenderer::draw_completion(const application::EditorFrame &frame,
                                       const core::LayoutRect &bounds)
{
    if (!frame.completion.has_value())
    {
        // 閉じたら文字組みと配置と左端の覚えを捨てる（右クリックのメニューと同じ）。
        completion_layouts_.clear();
        completion_anchor_.reset();
        completion_layout_.reset();
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
    completion_layout_ = std::pair{layout, view.first};
    CompletionPainter painter(
        *context_.Get(), *brush_.Get(),
        ContextMenuText{dwrite_.Get(), command_format_.Get(), key_format_.Get(), dpi_},
        completion_layouts_);
    painter.paint(view, layout, frame.palette);
}
} // namespace nenenib::ui::win32
