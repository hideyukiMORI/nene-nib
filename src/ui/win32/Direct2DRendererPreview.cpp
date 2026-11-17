// Direct2DRenderer の探して見る面のプレビュー欄の群（ADR 0189 の決定 5・#1785 / #1793）。本体の
// 翻訳単位が CNF-014 の 3,000 行を越えたので、欄の書式・描画・面の開閉の後始末をここへ分けた。
#include "Direct2DRenderer.hpp"

#include "EditorFrame.hpp"
#include "PaletteLayout.hpp"
#include "PreviewPanePainter.hpp"
#include "PreviewPaneText.hpp"
#include "UiTextSizes.hpp"

#include <array>
#include <cstdint>

namespace nenenib::ui::win32
{
bool Direct2DRenderer::create_preview_pane_formats(const wchar_t *body_face)
{
    const wchar_t *interface_face = family(L"Segoe UI Variable Text", L"Segoe UI");
    if (FAILED(make_format(body_face, preview_line_dips, DWRITE_FONT_WEIGHT_NORMAL,
                           preview_line_format_)) ||
        FAILED(make_format(body_face, preview_line_dips, DWRITE_FONT_WEIGHT_NORMAL,
                           preview_number_format_)) ||
        FAILED(make_format(interface_face, preview_heading_dips, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                           preview_name_format_)) ||
        FAILED(make_format(interface_face, preview_heading_dips, DWRITE_FONT_WEIGHT_NORMAL,
                           preview_note_format_)))
    {
        return false;
    }
    const std::array<TextFormat, 4> every{preview_line_format_, preview_number_format_,
                                          preview_name_format_, preview_note_format_};
    for (const auto &format : every)
    {
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    preview_number_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    return SUCCEEDED(trim_by_character(preview_name_format_.Get())) &&
           SUCCEEDED(trim_by_character(preview_note_format_.Get()));
}

PreviewPaneText Direct2DRenderer::preview_pane_text() const
{
    return PreviewPaneText{dwrite_.Get(),
                           preview_line_format_.Get(),
                           preview_number_format_.Get(),
                           preview_name_format_.Get(),
                           preview_note_format_.Get(),
                           dpi_};
}

void Direct2DRenderer::draw_preview_pane(const application::EditorFrame &frame,
                                         const core::PaletteLayout &layout)
{
    if (!frame.command_palette.has_value())
    {
        return;
    }
    PreviewPanePainter painter(*context_.Get(), *brush_.Get(), preview_pane_text(),
                               preview_layouts_);
    painter.paint(frame.command_palette.value().preview, layout, frame.palette, frame.body);
}

// 面が開いていれば描き、閉じていれば行とプレビュー欄の文字組みを捨てる（#900・ADR 0189）。
void Direct2DRenderer::draw_open_palette(const application::EditorFrame &frame, std::int32_t width,
                                         std::int32_t height)
{
    if (frame.command_palette.has_value())
    {
        const auto &palette = frame.command_palette.value();
        draw_palette(frame,
                     core::palette_layout(width, height, dpi_, {palette.total, palette.placement}));
    }
    else
    {
        // 面を閉じたら行の文字組みを捨てる。開くたびに作り直す（#900）。プレビュー欄も同じ。
        palette_layouts_.clear();
        preview_layouts_.clear();
    }
}
} // namespace nenenib::ui::win32
