// 本文の右クリックのメニューの描画と当たり（ADR 0078 の追補 #1051）。Direct2DRenderer.cpp の
// 長さの上限（CNF-014）のため翻訳単位を分けた。
#include "Direct2DRenderer.hpp"

#include "ContextMenu.hpp"
#include "ContextMenuPainter.hpp"
#include "ContextMenuRequest.hpp"
#include "ContextMenuText.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
std::optional<std::size_t> Direct2DRenderer::context_menu_row_at(std::int32_t x,
                                                                 std::int32_t y) const
{
    return context_menu_layout_.has_value()
               ? core::context_menu_hit(context_menu_layout_.value(), x, y)
               : std::nullopt;
}

bool Direct2DRenderer::context_menu_covers(std::int32_t x, std::int32_t y) const
{
    return context_menu_layout_.has_value() &&
           core::contains(context_menu_layout_.value().panel, x, y);
}

void Direct2DRenderer::draw_context_menu(const application::EditorFrame &frame, std::int32_t width,
                                         std::int32_t height)
{
    if (!frame.context_menu.has_value())
    {
        // 閉じたら文字組みと配置を捨てる。開くたびに作り直す（Ctrl+P の面と同じ・#900）。
        context_menu_layouts_.clear();
        context_menu_layout_.reset();
        return;
    }
    const auto &view = frame.context_menu.value();
    ContextMenuPainter painter(
        *context_.Get(), *brush_.Get(),
        ContextMenuText{dwrite_.Get(), command_format_.Get(), key_format_.Get(), dpi_},
        context_menu_layouts_);
    const auto widths = painter.measure(view);
    context_menu_layout_ = core::context_menu_layout(core::ContextMenuRequest{
        view.x, view.y, core::LayoutRect{0, 0, width, height}, widths.name, widths.key, dpi_});
    painter.paint(view, context_menu_layout_.value(), frame.palette);
}
} // namespace nenenib::ui::win32
